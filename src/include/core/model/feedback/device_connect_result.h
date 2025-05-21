#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core::feedback {

struct DeviceConnectResult {
    std::string device_id;
    bool success = true;
    bool pin_code_error = false;
    bool network_error = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        DeviceConnectResult, device_id, success, pin_code_error, network_error);
};

} // namespace lansend::core::feedback