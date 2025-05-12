#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace lansend {
namespace models {

struct TransferProgress {
    uint64_t transfer_id;
    std::string filename;
    uint64_t bytes_transferred;
    uint64_t total_bytes;
    double percentage; // 0 -> 1.0

    // 构造函数或更新函数
    TransferProgress(uint64_t id, const std::string& name, uint64_t total)
        : transfer_id(id)
        , filename(name)
        , bytes_transferred(0)
        , total_bytes(total)
        , percentage(0.0) {}

    // 默认构造函数，用于反序列化
    TransferProgress() = default;

    void update(uint64_t transferred) {
        bytes_transferred = transferred;
        if (total_bytes > 0) {
            percentage = static_cast<double>(bytes_transferred) / total_bytes;
        }
    }

    // 序列化/反序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        TransferProgress, transfer_id, filename, bytes_transferred, total_bytes, percentage);
};

} // namespace models
} // namespace lansend