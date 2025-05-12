#pragma once

#include "../models/device_info.hpp"
#include "../models/transfer_metadata.hpp"
#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

enum class TransferStatus { Pending, InProgress, Completed, Failed, Cancelled };

struct TransferResult {
    bool success;
    std::string error_message;
    uint64_t transfer_id;
    std::chrono::system_clock::time_point end_time;
};

// struct TransferProgress {
//     uint64_t transfer_id;
//     uint64_t total_size;
//     uint64_t transferred_size;
//     double progress; // 0.0 to 1.0
// };

struct TransferState {
    uint64_t id;
    std::string source_device;
    std::string target_device;
    std::filesystem::path filepath;
    uint64_t total_size;
    uint64_t transferred_size;
    TransferStatus status;
    std::chrono::system_clock::time_point start_time;
};

class TransferManager {
public:
    TransferManager(boost::asio::io_context& ioc);
    ~TransferManager();

    // 传输控制
    std::future<TransferResult> start_transfer(const lansend::models::DeviceInfo& target,
                                               const std::filesystem::path& filepath);
    void cancel_transfer(uint64_t transfer_id);

    // 状态查询
    std::optional<TransferState> get_transfer_state(uint64_t transfer_id) const;
    std::vector<TransferState> get_active_transfers() const;
    std::optional<lansend::models::TransferMetadata> get_transfer_metadata(
        uint64_t transfer_id) const;

private:
    boost::asio::io_context& io_context_;

    std::map<uint64_t, TransferState> active_transfers_;
    std::atomic<uint64_t> next_transfer_id_{1};
};