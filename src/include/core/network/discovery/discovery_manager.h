#pragma once

#include "core/security/certificate_manager.h"
#include <boost/asio.hpp>
#include <chrono>
#include <core/model.h>
#include <core/util/config.h>
#include <core/util/logger.h>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lansend::core {

// 定义一个结构体来存储设备信息和最后活跃时间，用来管理删除长时间不活跃的设备
struct DeviceEntry {
    DeviceInfo device;
    std::chrono::system_clock::time_point last_seen;
};

class DiscoveryManager {
public:
    DiscoveryManager(boost::asio::io_context& ioc);
    ~DiscoveryManager();

    void Start(uint16_t port);
    void Stop();

    // 设备管理
    void AddDevice(const DeviceInfo& device);
    void RemoveDevice(const std::string& device_id);
    std::optional<DeviceInfo> GetDevice(const std::string& device_id) const;
    std::vector<DeviceInfo> GetDevices() const;

    // 为Electron预留的事件接口
    void SetDeviceFoundCallback(std::function<void(const DeviceInfo&)> callback);
    void SetDeviceLostCallback(std::function<void(std::string_view)> callback);

private:
    std::mutex devices_mutex_;
    boost::asio::io_context& io_context_;
    std::string device_id_;

    std::unordered_map<std::string, DeviceEntry> discovered_devices_;
    std::function<void(const DeviceInfo&)> device_found_callback_ = nullptr;
    std::function<void(std::string_view)> device_lost_callback_ = nullptr;

    // UDP相关
    boost::asio::ip::udp::socket broadcast_socket_;
    boost::asio::ip::udp::socket listen_socket_;
    boost::asio::steady_timer broadcast_timer_;
    boost::asio::steady_timer cleanup_timer_; // 新增清理定时器

    // 协程任务
    boost::asio::awaitable<void> broadcaster();
    boost::asio::awaitable<void> listener();

    boost::asio::awaitable<void> cleanupDevices();

    std::string generateDeviceId();
};

} // namespace lansend::core