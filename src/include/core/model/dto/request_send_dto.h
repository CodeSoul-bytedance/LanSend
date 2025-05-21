#pragma once

#include "../device_info.h"
#include "file_dto.h"
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <vector>

namespace lansend::core {

struct RequestSendDto {
    DeviceInfo device_info;     // 发送方的设备信息
    std::vector<FileDto> files; // 文件信息列表

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RequestSendDto, device_info, files);
};

} // namespace lansend::core
