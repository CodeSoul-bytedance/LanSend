#pragma once
#include "device_info.hpp"
#include "transfer_metadata.hpp"
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace lansend {
namespace models {

// 对 /send-request 的响应，包含接受的文件及其 token
// key: fileId, value: token
using SendRequestResponse = std::map<std::string, std::string>;

// 传输状态响应 (对应 /status/{transfer_id})
struct StatusResponse { // 对应 /status/{transfer_id}
    uint64_t transfer_id;
    std::string file_name;
    uint64_t file_size;
    uint64_t chunk_size;
    std::vector<uint64_t> completed_chunks;
    uint64_t total_chunks;
    TransferStatus status;

    // 序列化/反序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StatusResponse,
                                   transfer_id,
                                   file_name,
                                   file_size,
                                   chunk_size,
                                   completed_chunks,
                                   total_chunks,
                                   status);
};

// 通用响应 (用于 /accept, /reject, /finish, /cancel 等操作的响应)
struct GenericResponse { // 用于 /accept, /reject, /finish, /cancel
    TransferStatus status;
    std::string message;

    // 序列化/反序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GenericResponse, status, message);
};

// 设备信息响应 (对应 /info)
struct InfoResponse { // 对应 /info
    std::string device_id;
    std::string alias;
    std::string device_model;

    // 序列化/反序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(InfoResponse, device_id, alias, device_model);
};

} // namespace models
} // namespace lansend