#include "../utils/logger.hpp"
#include <boost/asio/use_awaitable.hpp>
#include <nlohmann/json.hpp> 
// #include <boost/asio/udp.hpp>
#include "discovery_manager.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <iostream>
#include <mutex>
#include <boost/asio/ip/udp.hpp>
#include "../models/device_info.hpp"
#include "../models/transfer_progress.hpp"
using json = nlohmann::json; 
using namespace boost::asio;

DiscoveryManager::DiscoveryManager(io_context& ioc)
    : io_context_(ioc),
      broadcast_socket_(ioc),
      listen_socket_(ioc),
      broadcast_timer_(ioc),
      device_found_callback_([](const lansend::models::DeviceInfo& device) {
          std::cout << "Device found: " << device.device_id << std::endl;
      }),
      device_lost_callback_([](const std::string& device_id) {
          std::cout << "Device lost: " << device_id << std::endl;
      }){}

DiscoveryManager::~DiscoveryManager() {
    stop();
}

void DiscoveryManager::start(uint16_t port) {
    try {
        // 初始化广播套接字
        broadcast_socket_.open(ip::udp::v4());
        broadcast_socket_.set_option(socket_base::broadcast(true));
        broadcast_socket_.set_option(socket_base::reuse_address(true));

        // 初始化监听套接字
        ip::udp::endpoint listen_endpoint(ip::udp::v4(), port);
        listen_socket_.open(listen_endpoint.protocol());
        listen_socket_.set_option(socket_base::reuse_address(true));
        listen_socket_.bind(listen_endpoint);

        // 启动广播和监听协程
        co_spawn(io_context_, broadcaster(), detached);
        co_spawn(io_context_, listener(), detached);
    } catch (const std::exception& e) {
        std::cerr << "Error starting DiscoveryManager: " << e.what() << std::endl;
    }
}

void DiscoveryManager::stop() {
    try {
        if (broadcast_socket_.is_open()) {
            broadcast_socket_.close();
        }
        if (listen_socket_.is_open()) {
            listen_socket_.close();
        }
        broadcast_timer_.cancel();
    } catch (const std::exception& e) {
        std::cerr << "Error stopping DiscoveryManager: " << e.what() << std::endl;
    }
}

void DiscoveryManager::add_device(const lansend::models::DeviceInfo& device) {
    // std::lock_guard<std::mutex> lock(devices_mutex_);
    auto it = discovered_devices_.find(device.device_id);
    if (it == discovered_devices_.end()) {
        discovered_devices_[device.device_id] = device;
        if (device_found_callback_) {
            device_found_callback_(device);
        }
    } 
    //else {
    //     it->second.last_seen = std::chrono::system_clock::now();
    // }
}

void DiscoveryManager::remove_device(const std::string& device_id) {
    // std::lock_guard<std::mutex> lock(devices_mutex_);
    auto it = discovered_devices_.find(device_id);
    if (it != discovered_devices_.end()) {
        discovered_devices_.erase(it);
        if (device_lost_callback_) {
            device_lost_callback_(device_id);
        }
    }
}

std::vector<lansend::models::DeviceInfo> DiscoveryManager::get_devices() const {
    // std::lock_guard<std::mutex> lock(devices_mutex_);
    std::vector<lansend::models::DeviceInfo> devices;
    for (const auto& pair : discovered_devices_) {
        devices.push_back(pair.second);
    }
    return devices;
}

void DiscoveryManager::set_device_found_callback(std::function<void(const lansend::models::DeviceInfo&)> callback) {
    device_found_callback_ = callback;
}

void DiscoveryManager::set_device_lost_callback(std::function<void(const std::string&)> callback) {
    device_lost_callback_ = callback;
}

awaitable<void> DiscoveryManager::broadcaster() {
    try {
        const std::string broadcast_address = "255.255.255.255";
        const uint16_t broadcast_port = 37020; // 示例端口
        ip::udp::endpoint broadcast_endpoint(ip::make_address(broadcast_address), broadcast_port);

        // 模拟设备信息
        lansend::models::DeviceInfo self_device;
        self_device.device_id = "self_device_id";
        self_device.alias = "Self Device";
        self_device.device_model = "win"; // 假设设备类型为 Windows
        self_device.ip_address = "127.0.0.1";
        self_device.port = 37020;

        // 使用 nlohmann/json 进行序列化
        json device_json = self_device;
        std::string data = device_json.dump();

        while (broadcast_socket_.is_open()) {
            co_await broadcast_socket_.async_send_to(buffer(data), broadcast_endpoint, use_awaitable);
            co_await broadcast_timer_.async_wait(use_awaitable);
            broadcast_timer_.expires_after(std::chrono::seconds(5)); // 每 5 秒广播一次
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in broadcaster: " << e.what() << std::endl;
    }
}

awaitable<void> DiscoveryManager::listener() {
    try {
        constexpr size_t buffer_size = 1024;
        std::array<char, buffer_size> recv_buffer;
        ip::udp::endpoint sender_endpoint;

        while (listen_socket_.is_open()) {
            size_t bytes_received = co_await listen_socket_.async_receive_from(
                buffer(recv_buffer), sender_endpoint, use_awaitable);

            std::string data(recv_buffer.data(), bytes_received);

            try {
                // 使用 nlohmann/json 进行反序列化
                json device_json = json::parse(data);
                lansend::models::DeviceInfo device = device_json.get<lansend::models::DeviceInfo>();

                add_device(device);
            } catch (const json::parse_error& e) {
                std::cerr << "Error parsing JSON data: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in listener: " << e.what() << std::endl;
    }
}