// Implemented based on PLAN.md, ARCHITECTURE.md and provided memory.
// Addressing lint errors from previous steps where possible.
// Implemented based on PLAN.md, ARCHITECTURE.md and provided memory.
// Addressing lint errors from previous steps where possible.
#include "transfer_manager.hpp"
#include "../models/device_info.hpp"
#include "../models/transfer_metadata.hpp"
#include "../models/transfer_request.hpp"
#include "../transfer/file_hasher.hpp"
#include "../utils/config.hpp"
#include "../utils/logger.hpp"
#include <algorithm>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/url.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <vector>

// Assumptions for lansend::Config methods used:
// std::string get_device_name() const;
// std::filesystem::path get_metadata_storage_path() const;
// uint64_t get_default_chunk_size() const;

// Assumptions for lansend::models::TransferMetadata (based on PLAN.md and memory):
// Constructor: TransferMetadata(uint64_t id, const std::string& name, uint64_t size, const std::string& hash, lansend::models::FileType type, uint64_t chunk_size, const std::string& original_path, std::optional<std::vector<uint8_t>> preview_data)
// Member method: bool save(const std::filesystem::path& storage_dir) const; // LINT: 'save' reported as not a member in previous feedback. Assuming it exists.
// Static method: static std::optional<TransferMetadata> load(const std::filesystem::path& storage_dir, uint64_t transfer_id); // LINT: 'load' reported as not a member. Assuming it exists.
// Enum lansend::models::FileType members like Generic, Image, Video // LINT: reported as not members. Assuming they exist as per memory.
// Metadata filename convention assumed: config.get_metadata_storage_path() / (std::to_string(transfer_id) + ".meta")

// Assumptions for lansend::FileHasher:
// Static Method: static std::string calculate_sha256_sync(const std::filesystem::path& filepath);

// Assumptions for lansend::models::DeviceInfo:
// Member: std::string device_id; (Changed from id() to device_id based on snake_case convention and lint feedback)

// HttpClient 实现
TransferManager::HttpClient::HttpClient(boost::asio::io_context& ioc) {
    // 初始化必要的成员变量，这里简化实现
    spdlog::debug("HttpClient initialized");
}

boost::asio::awaitable<TransferManager::HttpClient::Response> TransferManager::HttpClient::post(
    const std::string& url,
    const std::string& payload,
    const std::map<std::string, std::string>& headers) {
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    try {
        // 解析URL
        boost::urls::url parsed_url = boost::urls::parse_uri(url).value();
        std::string host = parsed_url.host();
        std::string port = parsed_url.port().empty() ? "80" : parsed_url.port();
        std::string target = parsed_url.path().empty() ? "/" : parsed_url.path();

        // 设置查询参数
        if (!parsed_url.query().empty()) {
            target += "?" + std::string(parsed_url.query());
        }

        // 解析IP和端口
        auto ex = co_await net::this_coro::executor;
        tcp::resolver resolver(ex);
        auto const results = co_await resolver.async_resolve(host, port, net::use_awaitable);

        // 创建和连接socket
        beast::tcp_stream stream(ex);
        co_await stream.async_connect(results, net::use_awaitable);

        // 构建HTTP请求
        http::request<http::string_body> req{http::verb::post, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "application/json");

        // 添加自定义标头
        for (const auto& [key, value] : headers) {
            req.set(key, value);
        }

        // 设置请求体
        req.body() = payload;
        req.prepare_payload();

        // 发送请求
        co_await http::async_write(stream, req, net::use_awaitable);

        // 接收响应
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        co_await http::async_read(stream, buffer, res, net::use_awaitable);

        // 关闭连接
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // 忽略未连接错误
        if (ec && ec != beast::errc::not_connected) {
            throw boost::system::system_error{ec};
        }

        // 返回响应
        Response response{static_cast<int>(res.result_int()), res.body()};
        co_return response;
    } catch (std::exception& e) {
        spdlog::error("HTTP客户端异常: {}", e.what());
        Response response{500, std::string("客户端错误: ") + e.what()};
        co_return response;
    }
}

TransferManager::TransferManager(boost::asio::io_context& ioc, lansend::Settings& config)
    : io_context_(ioc)
    , config_(config)
    , http_client_(ioc)
    , next_transfer_id_(1) {
    spdlog::info("TransferManager initialized.");
}

TransferManager::~TransferManager() {
    spdlog::info("TransferManager shutting down.");
}

boost::asio::awaitable<TransferResult> TransferManager::start_transfer(
    const lansend::models::DeviceInfo& target, const std::filesystem::path& filepath) {
    // 1. Validate filepath
    if (!std::filesystem::exists(filepath) || !std::filesystem::is_regular_file(filepath)) {
        std::string msg = fmt::format("File not found or is not a regular file: {}",
                                      filepath.string());
        spdlog::error(msg);
        TransferResult result_to_return;
        result_to_return.success = false;
        result_to_return.error_message = msg;
        result_to_return.transfer_id = 0;
        result_to_return.end_time = std::chrono::system_clock::now();
        co_return result_to_return;
    }

    // 2. Get file size
    uint64_t file_size = std::filesystem::file_size(filepath);

    // 3. Generate a unique transfer ID (e.g., using a timestamp or UUID)
    uint64_t transfer_id = next_transfer_id_++;

    // 4. Determine file type and generate preview (if applicable)
    lansend::models::FileType file_type = lansend::models::FileType::OTHER; // Default to GENERIC
    std::string extension = filepath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".gif"
        || extension == ".bmp" || extension == ".webp") {
        file_type = lansend::models::FileType::IMAGE;
    } else if (extension == ".mp4" || extension == ".avi" || extension == ".mkv"
               || extension == ".mov" || extension == ".webm") {
        file_type = lansend::models::FileType::VIDEO;
    } else if (extension == ".mp3" || extension == ".wav" || extension == ".ogg"
               || extension == ".aac" || extension == ".flac") {
        file_type = lansend::models::FileType::OTHER;
    } else if (extension == ".txt" || extension == ".doc" || extension == ".docx"
               || extension == ".pdf" || extension == ".ppt" || extension == ".pptx"
               || extension == ".xls" || extension == ".xlsx" || extension == ".md") {
        file_type = lansend::models::FileType::OTHER;
    } else if (extension == ".zip" || extension == ".rar" || extension == ".tar"
               || extension == ".gz" || extension == ".7z") {
        file_type = lansend::models::FileType::OTHER;
    }

    // Calculate file hash
    std::optional<std::string> calculated_file_hash = std::nullopt;
    try {
        calculated_file_hash = file_hasher_.calculate_sha256_sync(filepath);
    } catch (const std::exception& e) {
        spdlog::warn("Could not calculate hash for file {}: {}", filepath.string(), e.what());
    }

    std::optional<std::string> preview_data_str = std::nullopt;
    // TODO(optional): Populate preview_data_str if preview_data_bytes (was preview_data) is generated and converted
    // Example: if (preview_data_bytes.has_value()) { preview_data_str = util::to_base64(*preview_data_bytes); }

    bool proceed_with_transfer = true;
    TransferResult result_to_return;
    result_to_return.transfer_id = transfer_id;

    std::string extracted_metadata_filename;

    if (proceed_with_transfer) {
        lansend::models::TransferMetadata metadata_obj_scoped;
        metadata_obj_scoped.transfer_id = transfer_id;
        metadata_obj_scoped.file_name = filepath.filename().string();
        metadata_obj_scoped.file_size = file_size;
        metadata_obj_scoped.file_hash = calculated_file_hash;
        metadata_obj_scoped.file_type = file_type;
        metadata_obj_scoped.preview = preview_data_str;
        metadata_obj_scoped.source_device_id = config_.device_id;
        metadata_obj_scoped.target_device_id = target.device_id;
        metadata_obj_scoped.status = lansend::models::TransferStatus::PENDING;
        metadata_obj_scoped.chunk_size = config_.chunkSize;
        metadata_obj_scoped.total_chunks = (file_size > 0) ? (file_size + config_.chunkSize - 1)
                                                                 / config_.chunkSize
                                                           : 0;
        metadata_obj_scoped.local_filepath = filepath.string();
        metadata_obj_scoped.created_at = std::chrono::system_clock::now();
        metadata_obj_scoped.updated_at = std::chrono::system_clock::now();

        if (metadata_obj_scoped.total_chunks > 0) {
            metadata_obj_scoped.chunks.reserve(metadata_obj_scoped.total_chunks);
            for (uint64_t i = 0; i < metadata_obj_scoped.total_chunks; ++i) {
                metadata_obj_scoped.chunks.push_back({i, std::nullopt, false});
            }
        }
        if (metadata_obj_scoped.total_chunks == 0 && file_size == 0) {
            metadata_obj_scoped.status = lansend::models::TransferStatus::COMPLETED;
        }

        // Extract necessary data before metadata_obj_scoped is destructed
        extracted_metadata_filename = metadata_obj_scoped.file_name;

        // Persist metadata_obj_scoped
        try {
            std::filesystem::path metadata_dir = config_.metadataStoragePath;
            std::error_code ec;
            std::filesystem::create_directories(metadata_dir, ec);
            if (ec) {
                std::string msg = fmt::format("Failed to create metadata directory {}: {}",
                                              metadata_dir.string(),
                                              ec.message());
                spdlog::error(msg);
                result_to_return.error_message = msg;
                result_to_return.end_time = std::chrono::system_clock::now();
                proceed_with_transfer = false;
            }

            if (proceed_with_transfer) {
                std::filesystem::path metadata_file_path = metadata_dir
                                                           / (std::to_string(transfer_id) + ".meta");
                nlohmann::json metadata_json_for_persistence = metadata_obj_scoped;
                std::ofstream ofs_for_persistence(metadata_file_path, std::ios::binary);

                if (!ofs_for_persistence) {
                    std::string msg = fmt::format("Failed to open metadata file for writing: {}",
                                                  metadata_file_path.string());
                    spdlog::error(msg);
                    result_to_return.error_message = msg;
                    result_to_return.end_time = std::chrono::system_clock::now();
                    proceed_with_transfer = false;
                } else {
                    ofs_for_persistence << metadata_json_for_persistence.dump(4);
                    if (ofs_for_persistence.fail()) {
                        std::string msg
                            = fmt::format("Failed to write transfer metadata to file: {}",
                                          metadata_file_path.string());
                        spdlog::error(msg);
                        result_to_return.error_message = msg;
                        result_to_return.end_time = std::chrono::system_clock::now();
                        proceed_with_transfer = false;
                    }
                    ofs_for_persistence.close();
                    if (ofs_for_persistence.fail()) {
                        std::string msg = fmt::format("Error closing metadata file {}.",
                                                      metadata_file_path.string());
                        spdlog::error(msg);
                        result_to_return.error_message = msg;
                        result_to_return.end_time = std::chrono::system_clock::now();
                        proceed_with_transfer = false;
                    }
                }
            }
        } catch (const nlohmann::json::exception& je) {
            std::string msg = fmt::format("JSON error while saving transfer metadata for ID {}: {}",
                                          transfer_id,
                                          je.what());
            spdlog::error(msg);
            result_to_return.error_message = msg;
            result_to_return.end_time = std::chrono::system_clock::now();
            proceed_with_transfer = false;
        } catch (const std::filesystem::filesystem_error& fse) {
            std::string msg
                = fmt::format("Filesystem error while saving transfer metadata for ID {}: {}",
                              transfer_id,
                              fse.what());
            spdlog::error(msg);
            result_to_return.error_message = msg;
            result_to_return.end_time = std::chrono::system_clock::now();
            proceed_with_transfer = false;
        } catch (const std::exception& e) {
            std::string msg
                = fmt::format("Generic exception while saving transfer metadata for ID {}: {}",
                              transfer_id,
                              e.what());
            spdlog::error(msg);
            result_to_return.error_message = msg;
            result_to_return.end_time = std::chrono::system_clock::now();
            proceed_with_transfer = false;
        }
    }

    if (!proceed_with_transfer) {
        co_return result_to_return;
    }

    // 6. Add to active transfers
    TransferState state{transfer_id,
                        config_.alias,
                        target.device_id,
                        filepath,
                        file_size,
                        0,                       // transferred_size
                        TransferStatus::Pending, // Initial status
                        std::chrono::system_clock::now()};
    active_transfers_[transfer_id] = state;

    spdlog::info("Transfer {} for file '{}' ({} bytes) to target '{}' initiated locally. Hash: {}.",
                 transfer_id,
                 filepath.string(),
                 file_size,
                 target.device_id,
                 calculated_file_hash.value_or(""));

    // 7. Initiate actual network communication
    spdlog::debug("Preparing SendRequest for transfer_id: {}", transfer_id);

    std::string payload_str;
    { // 新增作用域，用于管理 SendRequest 及其相关临时对象的生命周期
        lansend::models::FileMetadataRequest local_file_meta_req;
        local_file_meta_req.id = std::to_string(transfer_id);
        local_file_meta_req.file_name = extracted_metadata_filename; // 来自先前创建的 TransferMetadata
        local_file_meta_req.size = file_size;
        local_file_meta_req.file_type = file_type;
        if (calculated_file_hash.has_value()) {
            local_file_meta_req.file_hash = calculated_file_hash.value();
        }

        lansend::models::DeviceInfo local_own_device_info;
        local_own_device_info.device_id = config_.device_id;
        local_own_device_info.alias = config_.alias;
        local_own_device_info.port = config_.port;
        // TODO: 设置适当的设备型号和IP地址
        local_own_device_info.device_model = "Windows"; // 或者适当的设备型号
        local_own_device_info.ip_address = "";          // 这里可能需要获取本机IP (保持原样)
        local_own_device_info.https = config_.https;    // 根据实际情况设置

        lansend::models::SendRequest local_send_payload;
        local_send_payload.info = local_own_device_info;
        local_send_payload.files[local_file_meta_req.id] = local_file_meta_req;

        // 创建一个更小范围的 nlohmann::json 对象
        nlohmann::json json_payload_obj = local_send_payload;
        payload_str = json_payload_obj.dump(); // 在此作用域内序列化为字符串
    } // local_file_meta_req, local_own_device_info, local_send_payload, 和 json_payload_obj 在此被销毁

    if (target.ip_address.empty()) {
        spdlog::error("Target IP address for device_id '{}' is empty. Cannot initiate transfer {}.",
                      target.device_id,
                      transfer_id);
        active_transfers_[transfer_id].status = TransferStatus::Failed;
        active_transfers_[transfer_id].error_message = "Target IP address is missing.";
        result_to_return.success = false;
        result_to_return.error_message = "Target IP address is missing.";
        result_to_return.end_time = std::chrono::system_clock::now();
        co_return result_to_return;
    }

    std::string scheme = "http";
    if (target.https) { // Assuming target.https is a bool indicating HTTPS support
        scheme = "https";
    }

    int effective_port = target.port;
    if (effective_port <= 0) {
        spdlog::warn("Target port for {} ({}) is invalid ({}). Defaulting to 53317.",
                     target.device_id,
                     target.ip_address,
                     target.port);
        effective_port = 53317; // Default LocalSend port
    }

    std::string url = fmt::format("{}://{}:{}/api/localsend/v2/send-request",
                                  scheme,
                                  target.ip_address,
                                  effective_port);

    spdlog::info("Sending transfer request for ID {} to URL: {} (File: '{}', Size: {} bytes)",
                 transfer_id,
                 url,
                 extracted_metadata_filename,
                 file_size);
    active_transfers_[transfer_id].status
        = TransferStatus::InProgress; // Update status before network call

    TransferResult final_result_from_network_op;            // 用于存储网络操作的结果
    final_result_from_network_op.transfer_id = transfer_id; // transfer_id 是确定的

    try {
        // 为 HTTP response 对象创建一个独立的作用域
        {
            auto response = co_await http_client_.post(url,
                                                       payload_str,
                                                       {{"Content-Type",
                                                         "application/json; charset=utf-8"}});

            if (response.status_code == 200) {
                spdlog::info("Target {} accepted transfer request for ID {}. Response body: {}",
                             target.device_id,
                             transfer_id,
                             response.body);
                // 更新 active_transfers_ 状态（如果需要，当前已是 InProgress）
                final_result_from_network_op.success = true;
                final_result_from_network_op.error_message
                    = "Transfer request sent successfully to target.";
            } else {
                std::string error_msg = fmt::format("Target rejected request (HTTP {}): {}",
                                                    response.status_code,
                                                    response.body);
                spdlog::error(
                    "Failed to send transfer request for ID {} to {}. Status: {}, Body: {}",
                    transfer_id,
                    url,
                    response.status_code,
                    response.body);
                active_transfers_[transfer_id].status = TransferStatus::Failed; // 根据错误更新状态
                active_transfers_[transfer_id].error_message = error_msg;
                final_result_from_network_op.success = false;
                final_result_from_network_op.error_message = error_msg;
            }
            // response 对象在此作用域结束时析构
        }

        final_result_from_network_op.end_time = std::chrono::system_clock::now();
        co_return final_result_from_network_op;

    } catch (const std::exception& e) {
        std::string error_msg = fmt::format("Network exception: {}", e.what());
        spdlog::error("Network exception while sending transfer request for ID {}: {}",
                      transfer_id,
                      e.what());
        active_transfers_[transfer_id].status = TransferStatus::Failed;
        active_transfers_[transfer_id].error_message = e.what(); // 记录原始异常信息

        // 为异常路径创建一个新的 TransferResult
        TransferResult result_to_return_on_exception;
        result_to_return_on_exception.success = false;
        result_to_return_on_exception.error_message = error_msg; // 返回格式化后的信息
        result_to_return_on_exception.transfer_id = transfer_id;
        result_to_return_on_exception.end_time = std::chrono::system_clock::now();
        co_return result_to_return_on_exception;
    } catch (...) {
        std::string error_msg = "Unknown network error during transfer initiation.";
        spdlog::error("Unknown network exception while sending transfer request for ID {}",
                      transfer_id);
        active_transfers_[transfer_id].status = TransferStatus::Failed;
        active_transfers_[transfer_id].error_message = error_msg;

        TransferResult result_to_return_on_unknown_exception;
        result_to_return_on_unknown_exception.success = false;
        result_to_return_on_unknown_exception.error_message = error_msg;
        result_to_return_on_unknown_exception.transfer_id = transfer_id;
        result_to_return_on_unknown_exception.end_time = std::chrono::system_clock::now();
        co_return result_to_return_on_unknown_exception;
    }
}

void TransferManager::cancel_transfer(uint64_t transfer_id) {
    spdlog::info("Attempting to cancel transfer {}.", transfer_id);
    auto it = active_transfers_.find(transfer_id);
    if (it != active_transfers_.end()) {
        if (it->second.status == TransferStatus::Completed
            || it->second.status == TransferStatus::Failed
            || it->second.status == TransferStatus::Cancelled) {
            spdlog::warn("Transfer {} is already in a final state ({}). Cannot cancel.",
                         transfer_id,
                         static_cast<int>(it->second.status));
            return;
        }

        it->second.status = TransferStatus::Cancelled;
        it->second.transferred_size = 0; // Reset transferred size on cancellation

        spdlog::info("Transfer {} status set to Cancelled.", transfer_id);

        try {
            // 获取传输元数据以了解目标设备信息
            std::optional<lansend::models::TransferMetadata> metadata = get_transfer_metadata(
                transfer_id);
            if (metadata) {
                // 确定目标设备ID
                std::string target_device_id;
                if (metadata->source_device_id == config_.device_id) {
                    // 如果我们是发送方，目标是target_device_id
                    target_device_id = metadata->target_device_id;
                } else {
                    // 如果我们是接收方，目标是source_device_id
                    target_device_id = metadata->source_device_id;
                }

                // 尝试从设备发现缓存中找到目标设备的IP和端口
                // 注意：在实际实现中，你可能需要从服务发现管理器获取设备信息
                // 这里简单假设我们知道目标设备的信息（从活动传输状态中）
                std::string target_ip;
                uint16_t target_port = 0;

                // 从传输状态中获取目标设备信息
                if (it->second.target_device == target_device_id) {
                    // 目标设备是接收方，尝试查找它的IP/端口
                    // 在实际实现中，这应该从设备发现缓存获取
                    spdlog::warn("Cannot find target device IP/port for cancel notification");
                    // 在此处缺少目标设备信息时，我们应该继续处理，而不是直接返回
                } else {
                    // 尝试发送取消请求到对方设备
                    if (!target_ip.empty() && target_port > 0) {
                        // 构建请求URL和有效载荷
                        std::string url = fmt::format("http{}://{}:{}/cancel",
                                                      (config_.https ? "s" : ""),
                                                      target_ip,
                                                      target_port);

                        // 创建请求有效载荷（JSON格式）
                        nlohmann::json payload = {{"transfer_id", transfer_id}};

                        // 异步发送取消请求
                        // 注意：这是一个协程函数，但我们在非协程上下文中调用它
                        // 在实际实现中，你可能需要使用不同的方法来处理异步操作
                        boost::asio::co_spawn(
                            io_context_,
                            [this,
                             url,
                             payload_str = payload.dump()]() -> boost::asio::awaitable<void> {
                                try {
                                    auto response = co_await http_client_.post(url, payload_str);
                                    if (response.status_code == 200) {
                                        spdlog::info("Successfully notified peer about transfer "
                                                     "cancellation");
                                    } else {
                                        spdlog::warn("Failed to notify peer about transfer "
                                                     "cancellation: HTTP {}",
                                                     response.status_code);
                                    }
                                } catch (const std::exception& e) {
                                    spdlog::error(
                                        "Exception while notifying peer about cancellation: {}",
                                        e.what());
                                }
                                co_return;
                            },
                            boost::asio::detached);
                    }
                }
            } else {
                spdlog::warn(
                    "Could not find metadata for transfer {} to notify peer about cancellation",
                    transfer_id);
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception while trying to notify peer about transfer cancellation: {}",
                          e.what());
        }

        // Delete persistent metadata
        std::filesystem::path metadata_file_path = config_.metadataStoragePath
                                                   / (std::to_string(transfer_id) + ".meta");
        try {
            if (std::filesystem::exists(metadata_file_path)) {
                if (std::filesystem::remove(metadata_file_path)) {
                    spdlog::info("Persistent metadata for transfer {} deleted from {}.",
                                 transfer_id,
                                 metadata_file_path.string());
                } else {
                    spdlog::error("Failed to delete persistent metadata for transfer {} from {}.",
                                  transfer_id,
                                  metadata_file_path.string());
                }
            } else {
                spdlog::warn(
                    "Persistent metadata for transfer {} not found at {}. Nothing to delete.",
                    transfer_id,
                    metadata_file_path.string());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            spdlog::error(
                "Filesystem error while deleting persistent metadata for transfer {}: {}. Path: {}",
                transfer_id,
                e.what(),
                metadata_file_path.string());
        } catch (const std::exception& e) {
            spdlog::error(
                "Exception while deleting persistent metadata for transfer {}: {}. Path: {}",
                transfer_id,
                e.what(),
                metadata_file_path.string());
        }

        // Optionally, remove from active_transfers_ map if not needed for history, or keep with Cancelled status.
        // For now, we keep it, and get_active_transfers will filter based on status.
    } else {
        spdlog::warn("Attempted to cancel non-existent transfer {}.", transfer_id);
    }
}

std::vector<TransferState> TransferManager::get_active_transfers() const {
    std::vector<TransferState> current_transfers;
    for (const auto& pair : active_transfers_) {
        // Define what constitutes an "active" transfer.
        // E.g., Pending, InProgress. Exclude Completed, Failed, Cancelled.
        if (pair.second.status == TransferStatus::Pending
            || pair.second.status == TransferStatus::InProgress) {
            current_transfers.push_back(pair.second);
        }
    }
    return current_transfers;
}

std::optional<lansend::models::TransferMetadata> TransferManager::get_transfer_metadata(
    uint64_t transfer_id) const {
    try {
        auto metadata = lansend::models::TransferMetadata::load(config_.metadataStoragePath,
                                                                transfer_id);
        if (metadata) {
            spdlog::debug("Metadata loaded for transfer id {}", transfer_id);
        }
        return metadata;
    } catch (const std::exception& e) {
        spdlog::error("Exception loading metadata for transfer {}: {}", transfer_id, e.what());
        return std::nullopt;
    }
}

std::optional<TransferState> TransferManager::get_transfer_state(uint64_t transfer_id) const {
    auto it = active_transfers_.find(transfer_id);
    if (it != active_transfers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::vector<char>> TransferManager::get_file_chunk(uint64_t transfer_id,
                                                                 uint64_t chunk_index,
                                                                 std::string& error_message_out) {
    auto it = active_transfers_.find(transfer_id);
    if (it == active_transfers_.end()) {
        error_message_out = fmt::format("Transfer ID {} not found or not active.", transfer_id);
        spdlog::warn(error_message_out);
        return std::nullopt;
    }
    const auto& transfer_state = it->second;

    // Load TransferMetadata using the path from settings_
    // The memory [72ab4098-62be-4b97-90c7-7d2ba055b421] confirms TransferMetadata::load
    // and settings_.get_metadata_storage_path()
    auto metadata_opt = lansend::models::TransferMetadata::load(config_.metadataStoragePath,
                                                                transfer_id);
    if (!metadata_opt) {
        error_message_out = fmt::format(
            "Failed to load metadata for transfer ID {}. Ensure metadata file exists at '{}'.",
            transfer_id,
            config_.metadataStoragePath.string());
        spdlog::warn(error_message_out);
        return std::nullopt;
    }
    const auto& metadata = metadata_opt.value();

    if (metadata.chunk_size == 0) {
        error_message_out = fmt::format("Invalid chunk_size (0) in metadata for transfer ID {}.",
                                        transfer_id);
        spdlog::error(error_message_out);
        return std::nullopt;
    }

    uint64_t offset_in_file = chunk_index * metadata.chunk_size;

    // If requested offset is at or beyond EOF, there's no data to send for this chunk.
    if (offset_in_file >= metadata.file_size) {
        spdlog::info("Requested chunk_index {} for transfer_id {} starts at or beyond EOF (offset "
                     "{}, file size {}). Returning empty chunk.",
                     chunk_index,
                     transfer_id,
                     offset_in_file,
                     metadata.file_size);
        return std::vector<char>(); // Empty vector signifies no more data for this or subsequent chunks
    }

    uint64_t bytes_remaining_in_file = metadata.file_size - offset_in_file;
    uint64_t actual_chunk_size_to_read = std::min<uint64_t>(metadata.chunk_size,
                                                            bytes_remaining_in_file);

    // This case should be covered by (offset_in_file >= metadata.size) but as a safeguard or if logic changes.
    if (actual_chunk_size_to_read == 0) {
        spdlog::info("No data to read (actual_chunk_size_to_read is 0) for chunk_index {} of "
                     "transfer_id {}. Returning empty chunk.",
                     chunk_index,
                     transfer_id);
        return std::vector<char>();
    }

    const std::filesystem::path& file_to_read_path = transfer_state.filepath;
    std::ifstream file_stream(file_to_read_path, std::ios::binary);

    if (!file_stream.is_open()) {
        error_message_out = fmt::format("Failed to open file '{}' for transfer ID {}. Error: {}",
                                        file_to_read_path.string(),
                                        transfer_id,
                                        strerror(errno));
        spdlog::error(error_message_out);
        return std::nullopt;
    }

    file_stream.seekg(offset_in_file, std::ios::beg);
    if (file_stream.fail()) {
        error_message_out
            = fmt::format("Failed to seek to offset {} in file '{}' for transfer ID {}. Error: {}",
                          offset_in_file,
                          file_to_read_path.string(),
                          transfer_id,
                          strerror(errno));
        spdlog::error(error_message_out);
        file_stream.close();
        return std::nullopt;
    }

    std::vector<char> chunk_data(actual_chunk_size_to_read);
    file_stream.read(chunk_data.data(), actual_chunk_size_to_read);

    if (static_cast<uint64_t>(file_stream.gcount()) != actual_chunk_size_to_read) {
        error_message_out = fmt::format("Error reading chunk {} for transfer ID {}. Expected {} "
                                        "bytes, read {}. File: '{}'. Error: {}",
                                        chunk_index,
                                        transfer_id,
                                        actual_chunk_size_to_read,
                                        file_stream.gcount(),
                                        file_to_read_path.string(),
                                        strerror(errno));
        spdlog::error(error_message_out);
        file_stream.close();
        // Depending on desired behavior, could return partially read data or nullopt.
        // For strictness, returning nullopt if full expected chunk isn't read.
        return std::nullopt;
    }

    file_stream.close();
    spdlog::debug("Successfully read chunk {} ({} bytes) for transfer ID {}. File: '{}'",
                  chunk_index,
                  actual_chunk_size_to_read,
                  transfer_id,
                  file_to_read_path.string());
    return chunk_data;
}