#pragma once

#include "../models/device_info.hpp"
#include "../utils/config.hpp"
#include "../utils/logger.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <mutex>


class DiscoveryManager {
public:
    // struct DeviceInfo {
    //     std::string id;
    //     std::string name;
    //     std::string ip;
    //     uint16_t port;
    //     std::string fingerprint;
    //     std::chrono::system_clock::time_point last_seen;
    // };

    DiscoveryManager(boost::asio::io_context& ioc);
    ~DiscoveryManager();

    void start(uint16_t port);
    void stop();

    // 设备管理
    void add_device(const lansend::models::DeviceInfo& device);
    void remove_device(const std::string& device_id);
    std::vector<lansend::models::DeviceInfo> get_devices() const;

    // 为Electron预留的事件接口
    void set_device_found_callback(std::function<void(const lansend::models::DeviceInfo&)> callback);
    void set_device_lost_callback(std::function<void(const std::string&)> callback);

private:
    std::mutex devices_mutex_;
    boost::asio::io_context& io_context_;

    std::map<std::string, lansend::models::DeviceInfo> discovered_devices_;
    std::function<void(const lansend::models::DeviceInfo&)> device_found_callback_;
    std::function<void(const std::string&)> device_lost_callback_;

    // UDP相关
    boost::asio::ip::udp::socket broadcast_socket_;
    boost::asio::ip::udp::socket listen_socket_;
    boost::asio::steady_timer broadcast_timer_;

    // 协程任务
    boost::asio::awaitable<void> broadcaster();
    boost::asio::awaitable<void> listener();
};