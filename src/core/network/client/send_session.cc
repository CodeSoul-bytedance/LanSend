#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <core/constant/route.h>
#include <core/constant/transfer.h>
#include <core/model.h>
#include <core/network/client/send_session.h>
#include <core/util/binary_message.h>
#include <fstream>
#include <spdlog/spdlog.h>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lansend::core {

SendSession::SendSession(boost::asio::io_context& ioc, CertificateManager& cert_manager)
    : ioc_(ioc)
    , client_(ioc, cert_manager)
    , cert_manager_(cert_manager) {}

void SendSession::Cancel() {
    spdlog::info("Try to cancel send session: {}", session_id_);
    net::co_spawn(ioc_, cancelSend(), net::detached);
}

bool SendSession::IsCancelled() const {
    return session_status_ == SessionStatus::kCancelledBySender
           || session_status_ == SessionStatus::kCancelledByReceiver;
}

boost::asio::awaitable<bool> SendSession::cancelSend() {
    spdlog::debug("SendSession::CancelSend");
    try {
        json data;
        data["session_id"] = session_id_;

        auto req = client_.CreateRequest<http::string_body>(http::verb::post,
                                                            ApiRoute::kCancelSend.data(),
                                                            false);
        req.body() = data.dump();
        req.prepare_payload();

        auto res = co_await client_.SendRequest(req);

        if (res.result() != http::status::ok) {
            spdlog::error("Failed to cancel send: {}:{}",
                          std::string_view(res.reason()),
                          res.body());
            co_return false;
        }

        session_status_ = SessionStatus::kCancelledBySender;
        co_return true;
    } catch (const std::exception& e) {
        spdlog::error("Error cancelling file transfer: {}", e.what());
        co_return false;
    }
}

std::vector<FileDto> SendSession::prepareFiles(const std::vector<std::filesystem::path>& file_paths) {
    std::vector<FileDto> prepared_files;
    for (const auto& file_path : file_paths) {
        if (fs::exists(file_path)) {
            FileDto file_dto;
            boost::uuids::random_generator uuid_gen;
            file_dto.file_id = boost::uuids::to_string(uuid_gen());
            spdlog::debug("File ID: {}", file_dto.file_id);
            file_dto.file_name = file_path.filename().string();
            file_dto.file_size = fs::file_size(file_path);
            file_dto.chunk_size = transfer::kDefaultChunkSize;
            file_dto.total_chunks = (file_dto.file_size + file_dto.chunk_size - 1)
                                    / file_dto.chunk_size;
            file_dto.file_checksum = FileHasher::CalculateFileChecksum(file_path);
            file_dto.file_type = GetFileType(file_path.string());
            spdlog::debug(
                "FileDto: file_id={}, file_name={}, file_size={}, chunk_size={}, total_chunks={}, "
                "file_checksum={}, file_type={}",
                file_dto.file_id,
                file_dto.file_name,
                file_dto.file_size,
                file_dto.chunk_size,
                file_dto.total_chunks,
                file_dto.file_checksum,
                FileTypeToString(file_dto.file_type));
            prepared_files.emplace_back(file_dto);
            transfer_files_.emplace(file_dto.file_id,
                                    TransferFileInfo{file_path,
                                                     file_dto.file_size,
                                                     file_dto.total_chunks,
                                                     ""});
        } else {
            spdlog::error("File not found: {}", file_path.string());
        }
    }
    return prepared_files;
}

boost::asio::awaitable<void> SendSession::Start(const std::vector<std::filesystem::path>& file_paths,
                                                std::string_view host,
                                                unsigned int port,
                                                SessionStartedCallback callback) {
    spdlog::debug("SendSession::Start");
    auto prepared_files = prepareFiles(file_paths);
    if (prepared_files.empty()) {
        spdlog::error("No files to send");
        co_return;
    }

    if (host.empty() || port == 0) {
        spdlog::error("Invalid host or port");
        co_return;
    }
    try {
        RequestSendDto send_request_dto;
        send_request_dto.device_info = DeviceInfo::LocalDeviceInfo();
        send_request_dto.files = std::move(prepared_files);

        bool connected = co_await client_.Connect(host, port);
        if (!connected) {
            spdlog::error("Failed to connect to server");
            session_status_ = SessionStatus::kFailed;
            co_return;
        }

        session_status_ = SessionStatus::kWaiting;

        // Local endpoint information for this specific connection
        // Different from the local device info which used for http server
        send_request_dto.device_info.ip_address
            = client_.local_endpoint().value().address().to_string();
        send_request_dto.device_info.port = client_.local_endpoint().value().port();

        do {
            bool accepted = co_await requestSend(send_request_dto);
            if (!accepted) {
                if (session_status_ == SessionStatus::kCancelledBySender
                    || session_status_ == SessionStatus::kCancelledByReceiver) {
                    spdlog::info("send request was cancelled");
                    co_return;
                } else if (session_status_ == SessionStatus::kWaiting) {
                    // Receiver is busy, wait for a while and retry
                    spdlog::info("Receiver is busy, retrying in 1 second...");
                    auto timer = net::steady_timer(co_await net::this_coro::executor,
                                                   std::chrono::seconds(1));
                    co_await timer.async_wait(net::use_awaitable);
                    continue;
                } else {
                    spdlog::info("send request to {}:{} was declined", host, port);
                    co_await client_.Disconnect();
                    session_status_ = SessionStatus::kDeclined;
                    co_return;
                }
            }
        } while (session_status_ == SessionStatus::kWaiting);

        spdlog::info("send request to {}:{} was accepted", host, port);
        if (callback) {
            callback();
        }

        session_status_ = SessionStatus::kSending;

        std::vector<std::pair<std::string, size_t>> files_by_size;
        for (const auto& [file_id, file_info] : transfer_files_) {
            files_by_size.emplace_back(file_id, file_info.file_size);
        }
        std::sort(files_by_size.begin(), files_by_size.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        spdlog::info("Sorted files by size:");
        for (const auto& [file_id, file_size] : files_by_size) {
            spdlog::info("File ID: {}, Size: {}", file_id, file_size);
        }
        spdlog::info("Start sending files");

        // Start sending files one by one in order of increasing size
        for (const auto& [file_id, _] : files_by_size) {
            co_await sendFile(file_id);
            if (session_status_ == SessionStatus::kCancelledBySender
                || session_status_ == SessionStatus::kCancelledByReceiver) {
                spdlog::info("File transfer cancelled");
                co_return;
            }
            if (session_status_ == SessionStatus::kFailed) {
                spdlog::info("Send session {} failed", session_id_);
                co_return;
            }
        }
        spdlog::info("All files sent successfully, closing session: {}", session_id_);
        session_status_ = SessionStatus::kCompleted;
    } catch (const std::exception& e) {
        if (session_status_ != SessionStatus::kCancelledBySender
            && session_status_ != SessionStatus::kCancelledByReceiver) {
            spdlog::error("Error starting session: {}", e.what());
            session_status_ = SessionStatus::kFailed;
        }
    }
}

net::awaitable<bool> SendSession::requestSend(const RequestSendDto& send_request_dto) {
    spdlog::debug("SendSession::SendRequest");
    try {
        json data = send_request_dto;
        auto req = client_.CreateRequest<http::string_body>(http::verb::post,
                                                            ApiRoute::kRequestSend.data(),
                                                            true);

        req.body() = data.dump();
        req.prepare_payload();

        spdlog::debug("Sending SendRequestDto: {}", req.body());
        auto res = co_await client_.SendRequest(req);

        if (session_status_ == SessionStatus::kCancelledBySender) {
            spdlog::debug("Sender cancelled waiting for user confirmation");
            co_return false;
        }

        if (res.result() != http::status::ok) {
            if (res.result() == http::status::forbidden) {
                spdlog::info("Send request is forbidden: {}", res.body());
                if (res.body() == "receiver busy") {
                    spdlog::info(
                        "The receiver is busy right now, automatically reject the request");
                    session_status_ = SessionStatus::kWaiting;
                } else if (res.body() == "declined") {
                    spdlog::info("Send request is cancelled by the sender");
                    session_status_ = SessionStatus::kDeclined;
                }
                co_return false;
            } else {
                throw std::runtime_error(
                    std::format("{}:{}", std::string_view(res.reason()), res.body()));
            }
        }

        json response_data = json::parse(res.body());
        RequestSendResponseDto response_dto;
        try {
            nlohmann::from_json(response_data, response_dto);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse SendRequestResponseDto: {}", e.what());
            session_status_ = SessionStatus::kFailed;
            co_return false;
        }

        // Check if the receiver really wants any files
        if (response_dto.file_tokens.empty()) {
            spdlog::info("Send request is rejected by the receiver");
            session_status_ = SessionStatus::kDeclined;
            co_return false;
        }

        session_id_ = std::move(response_dto.session_id);
        spdlog::info("Send Request is accepted, session_id: {}", session_id_);
        session_status_ = SessionStatus::kSending;

        for (auto& [file_id, file_token] : response_dto.file_tokens) {
            auto it = transfer_files_.find(file_id);
            if (it != transfer_files_.end()) {
                // Update file token and start sending the file
                it->second.file_token = std::move(file_token);
            } else {
                // Remove unwanted file
                spdlog::info("File {} is unwanted, remove it", file_id);
                transfer_files_.erase(it);
            }
        }

        co_return true;
    } catch (const std::exception& e) {
        spdlog::error("Error occurred on SendSession::SendRequest: {}", e.what());
        session_status_ = SessionStatus::kFailed;
        co_return false;
    }
}

boost::asio::awaitable<void> SendSession::sendFile(std::string_view file_id) {
    spdlog::debug("SendSession::SendFile");
    try {
        if (session_status_ == SessionStatus::kCancelledBySender
            || session_status_ == SessionStatus::kCancelledByReceiver) {
            co_return;
        }
        if (!client_.IsConnected()) {
            throw std::runtime_error("No active connection for sending chunk");
        }
        TransferFileInfo& file_info = transfer_files_.at(file_id.data());

        std::ifstream file(file_info.file_path, std::ios::binary);
        if (!file) {
            spdlog::error("Failed to open file: {}", file_info.file_path.string());
            throw std::runtime_error("Failed to open file");
        }

        for (std::size_t chunk_idx = 0; chunk_idx < file_info.total_chunks; ++chunk_idx) {
            std::size_t current_chunk_size = std::min(transfer::kDefaultChunkSize,
                                                      file_info.file_size
                                                          - chunk_idx * transfer::kDefaultChunkSize);
            BinaryData chunk_data(current_chunk_size);

            file.read(reinterpret_cast<char*>(chunk_data.data()), transfer::kDefaultChunkSize);

            if (file.gcount() == 0) {
                break;
            }

            SendChunkDto send_chunk_dto{
                session_id_,
                file_id.data(),
                file_info.file_token,
                chunk_idx,
                FileHasher::CalculateDataChecksum(chunk_data),
            };

            bool chunk_sent = co_await sendChunk(send_chunk_dto, chunk_data);
            // judge if send is cancelled
            if (!chunk_sent) {
                if (session_status_ == SessionStatus::kCancelledBySender
                    || session_status_ == SessionStatus::kCancelledByReceiver) {
                    spdlog::info("File transfer cancelled");
                } else {
                    spdlog::error("Failed to send chunk {}/{} of file {}",
                                  chunk_idx + 1,
                                  file_info.total_chunks,
                                  file_id);
                    session_status_ = SessionStatus::kFailed;
                }
                file.close();
                co_return;
            }

            if ((chunk_idx + 1) % 10 == 0 || chunk_idx + 1 == file_info.total_chunks) {
                spdlog::info("Sent chunk {}/{} ({:.1f}%)",
                             chunk_idx + 1,
                             file_info.total_chunks,
                             100.0 * (chunk_idx + 1) / file_info.total_chunks);
            }
        }

        file.close();

        spdlog::info("File {} sent successfully", file_info.file_path.string());
        bool finalized = co_await verifyIntegrity(
            {session_id_, file_id.data(), file_info.file_token});
        if (!finalized) {
            if (session_status_ == SessionStatus::kCancelledBySender
                || session_status_ == SessionStatus::kCancelledByReceiver) {
                spdlog::info("File transfer cancelled");
            } else {
                spdlog::error("Verification failed for file {}", file_info.file_path.string());
                session_status_ = SessionStatus::kFailed;
            }
        } else {
            spdlog::info("File {} verification completed successfully",
                         file_info.file_path.string());
        }
    } catch (const std::exception& e) {
        if (session_status_ != SessionStatus::kCancelledBySender
            && session_status_ != SessionStatus::kCancelledByReceiver) {
            spdlog::error("Error occurred on SendSession::SendFile: {}", e.what());
            session_status_ = SessionStatus::kFailed;
        }
    }
}

net::awaitable<bool> SendSession::sendChunk(const SendChunkDto& send_chunk_dto,
                                            const BinaryData& chunk_data) {
    spdlog::debug("SendSession::SendChunk");
    try {
        if (session_status_ == SessionStatus::kCancelledBySender) {
            co_return false;
        }

        json metadata = send_chunk_dto;

        BinaryMessage binary_message = CreateBinaryMessage(metadata, chunk_data);

        auto req = client_.CreateRequest<http::vector_body<uint8_t>>(http::verb::post,
                                                                     ApiRoute::kSendChunk.data(),
                                                                     true);

        req.body() = std::move(binary_message);
        req.prepare_payload();

        auto res = co_await client_.SendRequest(req);
        // Check if the session is cancelled by sender
        // Since status modification takes place parallelly to this co_await
        if (session_status_ == SessionStatus::kCancelledBySender) {
            co_return false;
        }

        if (res.result() == http::status::ok) {
            spdlog::debug("Chunk {} sent successfully", send_chunk_dto.current_chunk_index);
            co_return true;
        } else if (res.result() == http::status::forbidden && res.body() == "receiver cancelled") {
            spdlog::info("File transfer cancelled by receiver");
            session_status_ = SessionStatus::kCancelledByReceiver;
            co_return false;
        } else {
            throw std::runtime_error(
                std::format("{}:{}", std::string_view(res.reason()), res.body()));
        }
    } catch (const std::exception& e) {
        if (session_status_ == SessionStatus::kCancelledBySender
            || session_status_ == SessionStatus::kCancelledByReceiver) {
            co_return false;
        } else {
            spdlog::error("Error occurred on SendSession::SendChunk: {}", e.what());
            co_return false;
        }
    }
}

net::awaitable<bool> SendSession::verifyIntegrity(const VerifyIntegrityDto& verify_integrity_dto) {
    spdlog::debug("SendSession::VerifyIntegrity");
    try {
        if (session_status_ == SessionStatus::kCancelledBySender) {
            co_return false;
        }

        json metadata = verify_integrity_dto;

        auto req = client_.CreateRequest<http::string_body>(http::verb::post,
                                                            ApiRoute::kVerifyIntegrity.data(),
                                                            true);

        req.body() = metadata.dump();
        req.prepare_payload();

        auto res = co_await client_.SendRequest(req);
        if (session_status_ == SessionStatus::kCancelledBySender) {
            co_return false;
        }

        if (res.result() == http::status::ok) {
            spdlog::debug("File integrity verification completed successfully");
            co_return true;
        } else if (res.result() == http::status::forbidden && res.body() == "receiver cancelled") {
            spdlog::info("File transfer cancelled by receiver");
            session_status_ = SessionStatus::kCancelledByReceiver;
            co_return false;
        } else {
            throw std::runtime_error(
                std::format("{}:{}", std::string_view(res.reason()), res.body()));
        }
    } catch (const std::exception& e) {
        if (session_status_ != SessionStatus::kCancelledBySender
            && session_status_ == SessionStatus::kCancelledByReceiver) {
            spdlog::error("Error occurred on SendSession::VerifyIntegrity: {}", e.what());
        }
        co_return false;
    }
}

} // namespace lansend::core