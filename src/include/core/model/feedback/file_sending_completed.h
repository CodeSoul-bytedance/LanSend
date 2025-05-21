#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core::feedback {

struct FileSendingCompleted {
    std::string session_id;
    std::string filename;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileSendingCompleted, session_id, filename);
};

} // namespace lansend::core::feedback