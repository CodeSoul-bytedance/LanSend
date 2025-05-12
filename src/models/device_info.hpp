#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace lansend {
namespace models {

struct DeviceInfo {
    std::string device_id;    // 唯一ID
    std::string alias;        // 设备昵称
    std::string device_model; // 设备类型(linux、win、mac或者更详细)
    std::string ip_address;
    uint16_t port;

    // 序列化/反序列化函数
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DeviceInfo, device_id, alias, device_model, ip_address, port);
};

} // namespace models
} // namespace lansend