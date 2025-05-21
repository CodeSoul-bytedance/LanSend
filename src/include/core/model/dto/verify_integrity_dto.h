#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core {

struct VerifyIntegrityDto {
    std::string session_id; // 会话唯一标识符
    std::string file_id;    // 文件唯一标识符
    std::string file_token; // 文件令牌

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(VerifyIntegrityDto, session_id, file_id, file_token);
};

} // namespace lansend::core