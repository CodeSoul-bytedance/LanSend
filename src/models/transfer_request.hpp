#pragma once
#include "device_info.hpp"
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace lansend {
namespace models {

// 文件类型
enum class FileType { IMAGE, VIDEO, PDF, TEXT, OTHER };

NLOHMANN_JSON_SERIALIZE_ENUM(FileType,
                             {{FileType::IMAGE, "image"},
                              {FileType::VIDEO, "video"},
                              {FileType::PDF, "pdf"},
                              {FileType::TEXT, "text"},
                              {FileType::OTHER, "other"}});

// 单个文件的元数据（用于请求）
struct FileMetadataRequest {
    std::string id;                       // 文件唯一标识符 (发送方生成)
    std::string file_name;                // 文件名
    uint64_t size;                        // 文件大小 (bytes)
    FileType file_type;                   // 文件类型
    std::optional<std::string> file_hash; // 文件哈希 (SHA256)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileMetadataRequest, id, file_name, size, file_type, file_hash);
};

// 文件发送请求 (包含发送者信息和文件列表)
struct SendRequest {
    DeviceInfo info;                                  // 发送方设备信息
    std::map<std::string, FileMetadataRequest> files; // 文件列表 (key 是 file id)

    // 注意：nlohmann/json 默认支持 std::map 的序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SendRequest, info, files);
};

// 文件传输控制请求 (如接受、拒绝、取消)
struct TransferControlRequest { // 用于 /accept, /reject, /finish, /cancel
    uint64_t transfer_id;       // 目标传输任务的ID
    // 序列化/反序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransferControlRequest, transfer_id);
};

} // namespace models
} // namespace lansend