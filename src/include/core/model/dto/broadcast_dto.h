#pragma once

#include "core/model/device_info.h"
namespace lansend::core {

struct BroadcastDto {
    DeviceInfo device_info;
    std::string fingerprint;
};

} // namespace lansend::core