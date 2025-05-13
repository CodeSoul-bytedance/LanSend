#include "transfer_manager.hpp"
#include <future>

TransferManager::TransferManager(boost::asio::io_context& ioc)
    : io_context_(ioc) {
    // 可以在这里初始化其他成员
}

TransferManager::~TransferManager() = default;

std::future<TransferResult> TransferManager::start_transfer(
    const lansend::models::DeviceInfo& target, const std::filesystem::path& filepath) {
    return std::async(std::launch::async, [this, target, filepath]() -> TransferResult {
        TransferResult result;
        // 实际传输逻辑应在这里实现
        result.success = true;
        result.error_message = "";
        result.transfer_id = next_transfer_id_++;
        result.end_time = std::chrono::system_clock::now();
        return result;
    });
}

std::vector<TransferState>& TransferManager::get_active_transfers() {
    // 假设 active_transfers_ 是 std::map<uint64_t, TransferState>
    // 你需要一个成员变量 std::vector<TransferState> active_transfers_vec_;
    // 或者临时构造一个 vector 返回引用（不推荐），
    // 推荐将 active_transfers_ 改为 std::vector<TransferState>，或者如下实现：

    // 如果 active_transfers_ 是 std::map
    static std::vector<TransferState> cache;
    cache.clear();
    for (auto& [id, state] : active_transfers_) {
        cache.push_back(state);
    }
    return cache;
}