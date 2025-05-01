#pragma once

#include "../discovery/discovery_manager.hpp"
#include "../util/config.hpp"
#include "../util/logger.hpp"
#include "transfer_metadata.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

class TransferManager {
public:
    enum class TransferStatus { Pending, InProgress, Completed, Failed, Cancelled, Paused };

    struct TransferResult {
        bool success;
        std::string error_message;
        uint64_t transfer_id;
        std::chrono::system_clock::time_point end_time;
        bool is_resumed; // 是否是断点续传完成的
    };

    struct TransferProgress {
        uint64_t transfer_id;
        uint64_t total_size;
        uint64_t transferred_size;
        double progress; // 0.0 to 1.0
        bool is_resumed; // 是否是断点续传
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
        bool is_resumable;                    // 是否支持断点续传
        std::filesystem::path temp_file_path; // 临时文件路径
        std::filesystem::path metadata_path;  // 元数据文件路径
    };

    TransferManager(boost::asio::io_context& ioc);
    ~TransferManager();

    // 传输控制
    boost::asio::awaitable<TransferResult> start_transfer(const DiscoveryManager::DeviceInfo& target,
                                                          const std::filesystem::path& filepath);

    // 断点续传
    boost::asio::awaitable<TransferResult> resume_transfer(uint64_t transfer_id);
    boost::asio::awaitable<bool> prepare_resumable_transfer(uint64_t transfer_id,
                                                            const std::string& filename,
                                                            uint64_t file_size,
                                                            const std::string& file_hash);
    boost::asio::awaitable<bool> receive_chunk(uint64_t transfer_id,
                                               uint64_t chunk_index,
                                               const std::vector<uint8_t>& data);

    void pause_transfer(uint64_t transfer_id);
    void cancel_transfer(uint64_t transfer_id);

    // 状态查询
    std::optional<TransferState> get_transfer_state(uint64_t transfer_id) const;
    std::vector<TransferState> get_active_transfers() const;
    std::vector<TransferState> get_incomplete_transfers() const;
    std::optional<uint64_t> find_incomplete_transfer(const std::string& filename,
                                                     uint64_t file_size) const;

private:
    boost::asio::io_context& io_context_;
    Config& config_;
    Logger& logger_;

    std::map<uint64_t, TransferState> active_transfers_;
    std::map<uint64_t, std::unique_ptr<TransferMetadata>> transfer_metadata_;
    std::atomic<uint64_t> next_transfer_id_{1};
    std::filesystem::path transfers_dir_;

    // 内部辅助方法
    void init_transfers_directory();
    std::filesystem::path get_metadata_path(uint64_t transfer_id, const std::string& filename) const;
    bool save_transfer_state(uint64_t transfer_id);
    bool load_transfer_state(uint64_t transfer_id);
    boost::asio::awaitable<bool> write_chunk_to_file(const std::filesystem::path& filepath,
                                                     uint64_t offset,
                                                     const std::vector<uint8_t>& data);
};