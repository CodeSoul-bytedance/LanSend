#pragma once

#include "core/model/device_info.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace lansend::core::feedback {

struct RequestReceiveFiles {
    DeviceInfo device_info;

    std::vector<std::string> file_names;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RequestReceiveFiles, device_info, file_names);
};

} // namespace lansend::core::feedback