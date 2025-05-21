#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core::feedback {

struct ReceiveSessionEnd {
    std::string session_id;
    bool success = true;
    bool cancelled_by_sender = false;
    std::string error_message;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        ReceiveSessionEnd, session_id, success, cancelled_by_sender, error_message);
};

} // namespace lansend::core::feedback