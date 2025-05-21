#include <algorithm>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/vector_body.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <core/constant/route.h>
#include <core/model.h>
#include <core/network/server/controller/receive_controller.h>
#include <core/network/server/http_server.h>
#include <core/util/binary_message.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ranges>
#include <regex>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lansend::core {

ReceiveController::ReceiveController(HttpServer& server,
                                     const std::filesystem::path& save_dir,
                                     FeedbackCallback callback)
    : server_(server)
    , save_dir_(save_dir)
    , callback_(callback) {
    if (!std::filesystem::exists(save_dir_)) {
        std::filesystem::create_directories(save_dir_);
    }
    installRoutes();
}

void ReceiveController::SetSaveDirectory(const std::filesystem::path& save_dir) {
    save_dir_ = save_dir;
    if (!std::filesystem::exists(save_dir_)) {
        std::filesystem::create_directories(save_dir_);
    }
}

ReceiveSessionStatus ReceiveController::session_status() const {
    return session_status_;
}

std::string_view ReceiveController::sender_ip() const {
    return sender_ip_;
}

unsigned short ReceiveController::sender_port() const {
    return sender_port_;
}

void ReceiveController::NotifySenderLost() {
    spdlog::info("Being notified that sender is lost before the session is completed");
    resetToIdle();

    // feedback session failed
    feedback(Feedback{
        .type = FeedbackType::kReceiveSessionEnded,
        .data = feedback::ReceiveSessionEnd{
            .session_id = session_id_,
            .success = false,
            .error_message = "Sender is lost",
        },
    });
}

void ReceiveController::SetFeedbackCallback(FeedbackCallback callback) {
    callback_ = callback;
}

void ReceiveController::SetWaitConditionFunc(WaitConditionFunc func) {
    wait_condition_ = func;
}

void ReceiveController::SetCancelConditionFunc(CancelConditionFunc func) {
    cancel_condition_ = func;
}

net::awaitable<http::response<http::string_body>> ReceiveController::onRequestSend(
    const http::request<http::string_body>& req) {
    spdlog::debug("ReceiveController::OnRequestSend");
    try {
        RequestSendDto request_send_dto;
        // NEW
        try {
            json data = json::parse(req.body());
            nlohmann::from_json(data, request_send_dto);
        } catch (const std::exception& e) {
            spdlog::error("Error parsing request: {}", e.what());
            co_return HttpServer::BadRequest(req.version(), req.keep_alive(), "invalid data");
        }

        DeviceInfo device_info = std::move(request_send_dto.device_info);
        std::vector<FileDto> files = std::move(request_send_dto.files);
        std::string all_file_names{};
        for (const auto& file : files) {
            all_file_names += std::format("{} ({})\n",
                                          file.file_name,
                                          FileTypeToString(file.file_type));
        }
        spdlog::info("{} {} ({}:{}) wants to send {} files:\n{}",
                     device_info.hostname,
                     device_info.operating_system,
                     device_info.ip_address,
                     device_info.port,
                     files.size(),
                     all_file_names);

        if (session_status_ != ReceiveSessionStatus::kIdle) {
            spdlog::info("The receiver is busy, automatically reject the request");
            co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "receiver busy");
        } else {
            spdlog::info("The receiver is idle, start handling the request");
            session_status_ = ReceiveSessionStatus::kWorking;
        }

        std::vector<std::string> file_names;
        for (const auto& file : files) {
            file_names.push_back(file.file_name);
        }

        // feedback session started
        feedback(Feedback{
            .type = FeedbackType::kRequestReceiveFiles,
            .data = feedback::RequestReceiveFiles{
                .device_info = device_info,
                .file_names = file_names,
            },
        });

        // Record sender's network information
        sender_ip_ = device_info.ip_address;
        sender_port_ = device_info.port;

        spdlog::debug("Wait for user confirmation");

        // Wait for user confirmation
        std::optional<std::vector<FileDto>> accepted_files
            = co_await waitForUserConfirmation(device_info.device_id, files, 30);

        // Sender might cancel waiting for user confirmation
        // resetToIdle() was called cocurrently when handling sender's request
        if (session_status_ != ReceiveSessionStatus::kWorking) {
            spdlog::info("Sender cancelled waiting for user confirmation");
            co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "sender cancelled");
        }

        spdlog::debug("Wait for user confirmation finished");

        if (accepted_files == std::nullopt) {
            spdlog::info("Send request is rejected by the receiver");
            resetToIdle();
            co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "declined");
        }

        // Generate a unique session ID with timestamp
        boost::uuids::random_generator uuid_gen;
        std::string timestamp = std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
        session_id_ = timestamp + boost::uuids::to_string(uuid_gen());
        spdlog::info("Send request accepted, generate session_id: {}", session_id_);

        // Create a session context and generate file tokens with file-specific information
        std::unordered_map<std::string, std::string> file_tokens;
        std::string receive_file_message;

        for (const auto& file : accepted_files.value()) {
            // Create a targeted token using file attributes
            std::string file_hash = std::to_string(
                std::hash<std::string>{}(file.file_name + file.file_id));
            std::string random_part = boost::uuids::to_string(uuid_gen()).substr(0, 12);
            std::string file_token = file_hash.substr(0, 8) + random_part;
            file_tokens[file.file_id] = file_token;

            // Create a temporary file path
            fs::path temp_file_path = save_dir_ / (file.file_id + ".part");

            // Add file to session context
            received_files_[file.file_id] = ReceiveFileContext{.file_name = file.file_name,
                                                               .temp_file_path = temp_file_path,
                                                               .file_token = file_token,
                                                               .file_size = file.file_size,
                                                               .chunk_size = file.chunk_size,
                                                               .total_chunks = file.total_chunks,
                                                               .received_chunks = {},
                                                               .file_checksum = file.file_checksum};

            // Build message about the file
            receive_file_message += std::format("{} ({} bytes), {} chunks expected\n",
                                                file.file_name,
                                                file.file_size,
                                                file.total_chunks);
        }
        spdlog::info("Started receiving {} files:\n{}",
                     accepted_files.value().size(),
                     receive_file_message);

        RequestSendResponseDto response_dto;
        response_dto.session_id = session_id_;
        response_dto.file_tokens = file_tokens;
        json response_data = response_dto;

        spdlog::info("Send request accepted, session_id: {}", session_id_);
        co_return HttpServer::Ok(req.version(), req.keep_alive(), response_data.dump());
    } catch (const std::exception& e) {
        spdlog::error("Error processing request: {}", e.what());
        resetToIdle();
        co_return HttpServer::InternalServerError(req.version(), req.keep_alive(), e.what());
    }
}

net::awaitable<http::response<http::string_body>> ReceiveController::onSendChunk(
    const http::request<http::vector_body<std::uint8_t>>& req) {
    spdlog::debug("ReceiveController::OnSendChunk");
    // Sender might still send several data when receiver received the cancellation request
    // and resetToIdle() was called cocurrently
    // Notify sender to stop the sending coroutine
    if (session_status_ != ReceiveSessionStatus::kWorking) {
        spdlog::info("Chunk data sent when receive session is already cancelled by sender");
        co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "sender cancelled");
    }

    // This should be polling the event stream to check the ui operation
    if (cancel_condition_()) {
        spdlog::info("receiver cancelled the session");
        resetToIdle();
        co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "receiver cancelled");
    }

    try {
        const BinaryMessage& binary_message = req.body();

        SendChunkDto send_chunk_dto;
        BinaryData chunk_data;
        try {
            json metadata;
            if (!ParseBinaryMessage(binary_message, metadata, chunk_data)) {
                throw std::runtime_error("Failed to parse binary message");
            }
            nlohmann::from_json(metadata, send_chunk_dto);
        } catch (const std::exception& e) {
            spdlog::error("Error parsing request: {}", e.what());
            co_return HttpServer::BadRequest(req.version(), req.keep_alive(), "invalid data");
        }

        // Check if the session ID matches
        if (send_chunk_dto.session_id != session_id_) {
            spdlog::error("Session ID mismatch: expected {}, got {}",
                          session_id_,
                          send_chunk_dto.session_id);
            throw std::runtime_error("Session ID mismatch");
        }

        // Check if file_id is valid
        if (auto iter = received_files_.find(send_chunk_dto.file_id);
            iter != received_files_.end()) {
            auto& file_context = iter->second;
            // Check if the file token matches
            if (file_context.file_token == send_chunk_dto.file_token) {
                // Check if the chunk has already been received
                if (file_context.received_chunks.contains(send_chunk_dto.current_chunk_index)) {
                    spdlog::warn("Chunk {} for file_id {} in session_id {} already received",
                                 send_chunk_dto.current_chunk_index,
                                 send_chunk_dto.file_id,
                                 send_chunk_dto.session_id);
                    co_return HttpServer::Ok(req.version(), req.keep_alive());
                }

                // All valid, process the chunk
                auto actual_checksum = FileHasher::CalculateDataChecksum(chunk_data);
                if (actual_checksum != send_chunk_dto.chunk_checksum) {
                    throw std::runtime_error(
                        std::format("Chunk checksum mismatch for file_id {} in session_id {}",
                                    send_chunk_dto.file_id,
                                    send_chunk_dto.session_id));
                }

                // Create or open the temporary file
                std::fstream temp_file(file_context.temp_file_path,
                                       std::ios::binary | std::ios::in | std::ios::out);
                if (!temp_file) {
                    temp_file.open(file_context.temp_file_path, std::ios::binary | std::ios::out);
                    if (!temp_file) {
                        throw std::runtime_error(
                            std::format("Failed to create temporary file {} for file {}",
                                        file_context.temp_file_path.string(),
                                        file_context.file_name));
                    }
                }

                std::size_t offset = send_chunk_dto.current_chunk_index * file_context.chunk_size;
                temp_file.seekp(offset);
                temp_file.write(reinterpret_cast<const char*>(chunk_data.data()), chunk_data.size());

                if (!temp_file) {
                    throw std::runtime_error(
                        std::format("Failed to write chunk to temporary file {} for file {}",
                                    file_context.temp_file_path.string(),
                                    file_context.file_name));
                }

                temp_file.close();

                // Update the received chunks count
                file_context.received_chunks.insert(send_chunk_dto.current_chunk_index);

                // feedback file receiving progress
                feedback(Feedback{
                    .type = FeedbackType::kFileReceivingProgress,
                    .data = feedback::FileReceivingProgress{
                        .session_id = session_id_,
                        .filename = file_context.file_name,
                        .progress = 
                            static_cast<double>(file_context.received_chunks.size())
                            / file_context.total_chunks * 100.0,
                    },
                });
                co_return HttpServer::Ok(req.version(), req.keep_alive(), "ok");
            } else {
                throw std::runtime_error(
                    std::format("Invalid file token for file_id {} in session_id {}",
                                send_chunk_dto.file_id,
                                send_chunk_dto.session_id));
            }
        } else {
            throw std::runtime_error(std::format("Invalid file_id {} in session_id {}",
                                                 send_chunk_dto.file_id,
                                                 send_chunk_dto.session_id));
        }

    } catch (const std::exception& e) {
        spdlog::error("Error processing chunk: {}", e.what());
        resetToIdle();

        // feedback session failed
        feedback(Feedback{
            .type = FeedbackType::kReceiveSessionEnded,
            .data = feedback::ReceiveSessionEnd{
                .session_id = session_id_,
                .success = false,
                .error_message = e.what(),
            },
        });
        co_return HttpServer::InternalServerError(req.version(), req.keep_alive(), e.what());
    }
}

net::awaitable<http::response<http::string_body>> ReceiveController::onVerifyIntegrity(
    const http::request<http::string_body>& req) {
    spdlog::debug("ReceiveController::OnVerifyIntegrity");

    if (session_status_ != ReceiveSessionStatus::kWorking) {
        spdlog::info("Chunk data sent when receive session is already cancelled by sender");
        co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "sender cancelled");
    }

    // This should be polling the event stream to check the ui operation
    if (cancel_condition_()) {
        spdlog::info("receiver cancelled the session");
        resetToIdle();
        co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "receiver cancelled");
    }

    try {
        VerifyIntegrityDto verify_integrity_dto;
        try {
            json data = json::parse(req.body());
            nlohmann::from_json(data, verify_integrity_dto);
        } catch (const std::exception& e) {
            spdlog::error("Error parsing request: {}", e.what());
            co_return HttpServer::BadRequest(req.version(), req.keep_alive(), "invalid data");
        }

        // Check if the session ID matches
        if (verify_integrity_dto.session_id != session_id_) {
            throw std::runtime_error(std::format("Session ID mismatch: expected {}, got {}",
                                                 session_id_,
                                                 verify_integrity_dto.session_id));
        }

        // Check if file_id is valid
        if (auto iter = received_files_.find(verify_integrity_dto.file_id);
            iter != received_files_.end()) {
            auto& file_context = iter->second;
            // Check if the file token matches
            if (file_context.file_token == verify_integrity_dto.file_token) {
                // Check if the file is complete
                if (file_context.received_chunks.size() != file_context.total_chunks) {
                    spdlog::error("File {} is not completely received ({} of {} chunks)",
                                  file_context.file_name,
                                  file_context.received_chunks.size(),
                                  file_context.total_chunks);
                    throw std::runtime_error(
                        std::format("File {} is not completely received ({} of {} chunks)",
                                    file_context.file_name,
                                    file_context.received_chunks.size(),
                                    file_context.total_chunks));
                }
                // Verify the file checksum
                auto actual_checksum = FileHasher::CalculateFileChecksum(
                    file_context.temp_file_path);
                if (actual_checksum != file_context.file_checksum) {
                    spdlog::debug("File checksum: {}, actual checksum: {}",
                                  file_context.file_checksum,
                                  actual_checksum);
                    throw std::runtime_error(
                        std::format("File checksum mismatch for file {} (id = {}) in session_id {}",
                                    file_context.file_name,
                                    verify_integrity_dto.file_id,
                                    verify_integrity_dto.session_id));
                }

                fs::path final_file_path = save_dir_ / file_context.file_name;
                // Add suffix if the file already exists
                if (fs::exists(final_file_path)) {
                    std::string stem = final_file_path.stem().string();
                    std::string ext = final_file_path.extension().string();
                    int counter = 1;

                    // Check if the filename already has the format "name (n)"
                    std::regex pattern(R"((.*) \((\d+)\)$)");
                    std::smatch matches;

                    if (std::regex_match(stem, matches, pattern)) {
                        // If it matches "name (n)" format, extract the base name and number
                        stem = matches[1].str();
                        counter = std::stoi(matches[2].str()) + 1;
                    }

                    // Try new filenames with increasing counter
                    do {
                        std::string new_stem = stem + " (" + std::to_string(counter) + ")";
                        final_file_path = save_dir_ / (new_stem + ext);
                        ++counter;
                    } while (fs::exists(final_file_path));
                }

                fs::rename(file_context.temp_file_path, final_file_path);

                spdlog::info("File {} received successfully, saved as \"{}\"",
                             file_context.file_name,
                             final_file_path.string());

                completed_file_count_++;

                // feedback file receiving completed
                feedback(Feedback{
                    .type = FeedbackType::kFileReceivingCompleted,
                    .data = feedback::FileReceivingCompleted{
                        .session_id = session_id_,
                        .filename = file_context.file_name,
                    },
                });

                // Check if all files in the session are completed
                checkSessionCompletion();

                co_return HttpServer::Ok(req.version(), req.keep_alive(), "ok");
            } else {
                throw std::runtime_error(
                    std::format("Invalid file token for file_id {} in session_id {}",
                                verify_integrity_dto.file_id,
                                verify_integrity_dto.session_id));
            }
        } else {
            throw std::runtime_error(std::format("Invalid file_id {} in session_id {}",
                                                 verify_integrity_dto.file_id,
                                                 verify_integrity_dto.session_id));
        }

    } catch (const std::exception& e) {
        spdlog::error("Error processing file integrity verification: {}", e.what());
        resetToIdle();

        // feedback session failed
        feedback(Feedback{
            .type = FeedbackType::kReceiveSessionEnded,
            .data = feedback::ReceiveSessionEnd{
                .session_id = session_id_,
                .success = false,
                .error_message = e.what(),
            },
        });
        co_return HttpServer::InternalServerError(req.version(), req.keep_alive(), e.what());
    }
}

net::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
ReceiveController::onCancelSend(const http::request<boost::beast::http::string_body>& req) {
    spdlog::debug("ReceiveController::OnCancelSend");
    if (session_status_ != ReceiveSessionStatus::kWorking) {
        spdlog::info(
            "cancel send request sent when receive session is already cancelled by sender");
        co_return HttpServer::Ok(req.version(), req.keep_alive(), "Not receiving");
    }

    try {
        std::string session_id;
        try {
            json data = json::parse(req.body());
            session_id = data["session_id"].get<std::string>();
        } catch (const std::exception& e) {
            spdlog::error("Error parsing request: {}", e.what());
            // NEW
            co_return HttpServer::BadRequest(req.version(), req.keep_alive(), "invalid data");
        }

        if (session_id == session_id_) {
            // Cancel the session
            resetToIdle();

            spdlog::info("Session {} is cancelled by the sender", session_id);

            // feedback session cancelled
            feedback(Feedback{
                .type = FeedbackType::kReceiveSessionEnded,
                .data = feedback::ReceiveSessionEnd{
                    .session_id = session_id_,
                    .success = false,
                    .cancelled_by_sender = true,
                },
            });

            co_return HttpServer::Ok(req.version(), req.keep_alive());
        } else {
            throw std::runtime_error("Invalid session ID: " + session_id);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing cancel request: {}", e.what());
        co_return HttpServer::InternalServerError(req.version(), req.keep_alive(), e.what());
    }
}

net::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
ReceiveController::onCancelWait(const http::request<boost::beast::http::string_body>& req) {
    spdlog::debug("ReceiveController::OnCancelWait");
    if (session_status_ != ReceiveSessionStatus::kWorking) {
        spdlog::info(
            "cancel wait request sent when receive session is already cancelled by sender");
        co_return HttpServer::Ok(req.version(), req.keep_alive(), "Not waiting");
    }

    try {
        std::string ip;
        unsigned short port;
        try {
            json data = json::parse(req.body());
            ip = data["ip"].get<std::string>();
            port = data["port"].get<unsigned short>();
        } catch (const std::exception& e) {
            spdlog::error("Error parsing request: {}", e.what());
            co_return HttpServer::BadRequest(req.version(), req.keep_alive(), "invalid data");
        }

        if (ip == sender_ip_ && port == sender_port_) {
            resetToIdle();
            spdlog::info("Wait for user confirmation is cancelled by the sender");

            // feedback session cancelled
            feedback(Feedback{
                .type = FeedbackType::kReceiveSessionEnded,
                .data = feedback::ReceiveSessionEnd{
                    .session_id = session_id_,
                    .success = false,
                    .cancelled_by_sender = true,
                },
            });
            co_return HttpServer::Ok(req.version(), req.keep_alive());
        } else {
            co_return HttpServer::Forbidden(req.version(),
                                            req.keep_alive(),
                                            "invalid sender ip or port");
        }

        co_return HttpServer::Ok(req.version(), req.keep_alive());
    } catch (const std::exception& e) {
        spdlog::error("Error processing cancel wait request: {}", e.what());
        co_return HttpServer::InternalServerError(req.version(), req.keep_alive(), e.what());
    }
}

boost::asio::awaitable<std::optional<std::vector<FileDto>>> ReceiveController::waitForUserConfirmation(
    std::string device_id, const std::vector<FileDto>& files, int timeout_seconds) {
    if (wait_condition_ == nullptr) {
        spdlog::warn("No wait condition function set, automatically accepting all files");
        co_return files;
    }

    auto executor = co_await net::this_coro::executor;
    std::optional<std::vector<FileDto>> result = std::nullopt;

    bool timeout = false;
    auto confirmation_task = [&]() -> net::awaitable<void> {
        auto start_time = std::chrono::steady_clock::now();
        while (session_status_ == ReceiveSessionStatus::kWorking) {
            if (auto filenames = wait_condition_(); filenames) {
                std::vector<FileDto> accepted_files;
                for (auto file : files) {
                    if (auto iter = std::ranges::find(filenames.value(), file.file_name);
                        iter != filenames->end()) {
                        accepted_files.emplace_back(file);
                    }
                }
                co_return;
            } else {
                auto duration = std::chrono::steady_clock::now() - start_time;
                if (std::chrono::duration_cast<std::chrono::seconds>(duration).count()
                    >= timeout_seconds) {
                    timeout = true;
                    spdlog::info("Timeout waiting for user confirmation, automatically declining");
                    co_return;
                }
                co_await net::post(executor);
            }
        }
    };

    try {
        co_await confirmation_task();
    } catch (const std::exception& e) {
        spdlog::error("Error waiting for user confirmation: {}", e.what());
    }

    co_return result;
}

void ReceiveController::installRoutes() {
    server_.AddRoute(ApiRoute::kRequestSend.data(),
                     http::verb::post,
                     std::bind(&ReceiveController::onRequestSend, this, std::placeholders::_1));
    server_.AddRoute(ApiRoute::kSendChunk.data(),
                     http::verb::post,
                     std::bind(&ReceiveController::onSendChunk, this, std::placeholders::_1));
    server_.AddRoute(ApiRoute::kVerifyIntegrity.data(),
                     http::verb::post,
                     std::bind(&ReceiveController::onVerifyIntegrity, this, std::placeholders::_1));
    server_.AddRoute(ApiRoute::kCancelSend.data(),
                     http::verb::post,
                     std::bind(&ReceiveController::onCancelSend, this, std::placeholders::_1));
}

void ReceiveController::doCleanup() {
    if (session_id_.empty() || received_files_.empty()) {
        return;
    }
    for (const auto& [file_id, file_context] : received_files_) {
        if (fs::exists(file_context.temp_file_path)) {
            spdlog::info("Cleaning up unfinished temp file of \"{}\"", file_context.file_name);
            fs::remove(file_context.temp_file_path);

            // Since the files are sent in order, we can assume that the last file is the one
            // that was being sent when the session was cancelled
            break;
        }
    }
}

void ReceiveController::checkSessionCompletion() {
    if (!session_id_.empty()) {
        if (!received_files_.empty() && completed_file_count_ == received_files_.size()) {
            spdlog::info("All files in session {} have been received successfully.", session_id_);
            resetToIdle();

            // feedback session completeds
            feedback(Feedback{
                .type = FeedbackType::kReceiveSessionEnded,
                .data = feedback::ReceiveSessionEnd{
                    .session_id = session_id_,
                    .success = true,
                },
            });
        } else {
            spdlog::info("Session {} is still in progress.", session_id_);
        }
    }
}

void ReceiveController::resetToIdle() {
    doCleanup();
    session_id_.clear();
    session_status_ = ReceiveSessionStatus::kIdle;
    received_files_.clear();
    completed_file_count_ = 0;
    sender_ip_.clear();
    sender_port_ = 0;
}

} // namespace lansend::core