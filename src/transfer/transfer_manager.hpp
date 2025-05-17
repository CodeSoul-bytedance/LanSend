#pragma once

#include "../models/device_info.hpp"
#include "../models/transfer_metadata.hpp"
#include "../utils/config.hpp"
#include "../utils/logger.hpp"
#include "file_hasher.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace lansend {
class Config;
}

enum class TransferStatus { Pending, InProgress, Completed, Failed, Cancelled };

struct TransferResult {
    bool success;
    std::string error_message;
    uint64_t transfer_id;
    std::chrono::system_clock::time_point end_time;
};

struct TransferState {
    uint64_t id;
    std::string source_device;
    std::string target_device;
    std::filesystem::path filepath;
    uint64_t total_size;
    uint64_t transferred_size;
    TransferStatus status;
    std::chrono::system_clock::time_point start_time;
    std::optional<std::string> error_message;
};

class TransferManager {
public:
    TransferManager(boost::asio::io_context& ioc, lansend::Settings& config);
    ~TransferManager();

    // 传输控制
    boost::asio::awaitable<TransferResult> start_transfer(const lansend::models::DeviceInfo& target,
                                                          const std::filesystem::path& filepath);
    void cancel_transfer(uint64_t transfer_id);

    // 状态查询
    std::optional<TransferState> get_transfer_state(uint64_t transfer_id) const;
    std::vector<TransferState> get_active_transfers() const;
    std::optional<lansend::models::TransferMetadata> get_transfer_metadata(
        uint64_t transfer_id) const;

    // Retrieves a specific chunk of a file for a given transfer.
    // Returns the chunk data or std::nullopt if an error occurs.
    // If an error occurs, error_message_out will contain a description of the error.
    std::optional<std::vector<char>> get_file_chunk(uint64_t transfer_id,
                                                    uint64_t chunk_index,
                                                    std::string& error_message_out);

private:
    boost::asio::io_context& io_context_;
    lansend::Settings& config_;
    FileHasher file_hasher_;

    // HTTP客户端用于向目标设备发送请求
    class HttpClient {
    public:
        struct Response {
            int status_code;
            std::string body;
        };

        HttpClient(boost::asio::io_context& ioc);

        // 发送POST请求的方法
        boost::asio::awaitable<Response> post(const std::string& url,
                                              const std::string& payload,
                                              const std::map<std::string, std::string>& headers = {});
    };

    HttpClient http_client_;
    std::map<uint64_t, TransferState> active_transfers_;
    std::atomic<uint64_t> next_transfer_id_{1};
};