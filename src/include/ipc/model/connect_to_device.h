#pragma once

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace lansend::ipc::operation {

struct ConnectToDevice {
    std::string device_id;
    std::string pin_code; // pin code for the device

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectToDevice, device_id, pin_code);
};

} // namespace lansend::ipc::operation