#pragma once

#include "core/model/device_info.h"
#include <nlohmann/json.hpp>
namespace lansend::core {

struct BroadcastDto {
    DeviceInfo device_info;
    std::string fingerprint;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BroadcastDto, device_info, fingerprint);
};

} // namespace lansend::core