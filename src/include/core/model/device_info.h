#pragma once
#include <core/util/config.h>
#include <core/util/system.h>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core {

struct DeviceInfo {
    std::string device_id;        // 唯一ID
    std::string hostname;         // 主机名
    std::string operating_system; // 操作系统
    std::string ip_address;
    uint16_t port;

    static auto& LocalDeviceInfo() {
        static DeviceInfo local_device_info = []() {
            DeviceInfo info;
            info.hostname = system::Hostname();
            info.operating_system = system::OperatingSystem();
            info.ip_address = "127.0.0.1";
            info.port = settings.port;

            // Generate a unique device ID using hash of hostname and OS and timestamp
            auto hash = std::hash<std::string>{}(
                info.hostname + info.operating_system
                + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
            info.device_id = std::to_string(hash);

            return info;
        }();
        auto addr = system::PublicIpv4Address();
        if (local_device_info.ip_address != addr) {
            local_device_info.ip_address = addr;
        }
        if (local_device_info.port != settings.port) {
            local_device_info.port = settings.port;
        }
        return local_device_info;
    }

    // 序列化/反序列化函数
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        DeviceInfo, device_id, hostname, operating_system, ip_address, port)
};

} // namespace lansend::core