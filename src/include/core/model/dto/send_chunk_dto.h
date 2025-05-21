#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core {

struct SendChunkDto {
    std::string session_id;     // 会话唯一标识符
    std::string file_id;        // 文件唯一标识符
    std::string file_token;     // 文件令牌
    size_t current_chunk_index; // 当前块编号
    std::string chunk_checksum; // 当前块的校验和

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        SendChunkDto, session_id, file_id, file_token, current_chunk_index, chunk_checksum);
};

} // namespace lansend::core