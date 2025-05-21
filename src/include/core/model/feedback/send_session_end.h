#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core::feedback {

struct SendSessionEnd {
    std::string session_id;
    std::string device_id;
    bool success = true;
    bool cancelled_by_receiver = false;
    std::string error_message;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        SendSessionEnd, session_id, success, cancelled_by_receiver, error_message);
};

} // namespace lansend::core::feedback