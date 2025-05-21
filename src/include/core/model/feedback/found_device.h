#pragma once

#include "core/model/device_info.h"

namespace lansend::core::feedback {

struct FoundDevice {
    DeviceInfo device_info;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FoundDevice, device_info);
};

} // namespace lansend::core::feedback