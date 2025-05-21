#pragma once

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace lansend::core {

struct RequestSendResponseDto {
    std::string session_id;                                   // 服务器生成的会话ID
    std::unordered_map<std::string, std::string> file_tokens; // 文件ID到令牌的映射

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RequestSendResponseDto, session_id, file_tokens);
};

} // namespace lansend::core