#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core::feedback {

struct LostDevice {
    std::string device_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LostDevice, device_id);
};

} // namespace lansend::core::feedback