#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <core/model.h>
#include <core/network/discovery/discovery_manager.h>
#include <random>
#include <spdlog/spdlog.h>
#include <string>

using namespace boost::asio;
using json = nlohmann::json;

namespace lansend::core {

DiscoveryManager::DiscoveryManager(io_context& ioc)
    : io_context_(ioc)
    , broadcast_socket_(ioc)
    , listen_socket_(ioc)
    , broadcast_timer_(ioc)
    , cleanup_timer_(ioc)
    , // 新增清理定时器
    device_found_callback_(
        [](const DeviceInfo& device) { spdlog::info("Device found:{}", device.device_id); })
    , device_lost_callback_(
          [](std::string_view device_id) { spdlog::info("Device lost:{}", device_id); }) {
    device_id_ = generateDeviceId();
    spdlog::info("discovery_manager created.");
}

DiscoveryManager::~DiscoveryManager() {
    Stop();
    spdlog::info("stop to discover devices.");
}

void DiscoveryManager::Start(uint16_t port) {
    try {
        spdlog::info("start to discover devices...");
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
        // 启动清理协程
        co_spawn(io_context_, cleanupDevices(), detached);
    } catch (const std::exception& e) {
        spdlog::error("Error starting DiscoveryManager: {}", e.what());
    }
}

void DiscoveryManager::Stop() {
    try {
        if (broadcast_socket_.is_open()) {
            broadcast_socket_.close();
            spdlog::info("broadcast socket is closed");
        }
        if (listen_socket_.is_open()) {
            listen_socket_.close();
            spdlog::info("listen socket is closed");
        }
        broadcast_timer_.cancel();
        cleanup_timer_.cancel();
        spdlog::info("broadcast timer is canceled");
    } catch (const std::exception& e) {
        spdlog::error("Error stopping DiscoveryManager: {}", e.what());
    }
}

void DiscoveryManager::AddDevice(const DeviceInfo& device) {
    // std::lock_guard<std::mutex> lock(devices_mutex_);
    auto it = discovered_devices_.find(device.device_id);
    if (it == discovered_devices_.end()) {
        discovered_devices_[device.device_id] = {device, std::chrono::system_clock::now()};
        if (device_found_callback_) {
            device_found_callback_(device);
        }
    } else {
        it->second.last_seen = std::chrono::system_clock::now();
    }
}

void DiscoveryManager::RemoveDevice(const std::string& device_id) {
    // std::lock_guard<std::mutex> lock(devices_mutex_);
    auto it = discovered_devices_.find(device_id);
    if (it != discovered_devices_.end()) {
        discovered_devices_.erase(it);
        if (device_lost_callback_) {
            device_lost_callback_(device_id);
        }
    }
}

std::optional<DeviceInfo> DiscoveryManager::GetDevice(const std::string& device_id) const {
    if (discovered_devices_.contains(device_id)) {
        return discovered_devices_.at(device_id).device;
    }
    return std::nullopt;
}

std::vector<DeviceInfo> DiscoveryManager::GetDevices() const {
    // std::lock_guard<std::mutex> lock(devices_mutex_);
    std::vector<DeviceInfo> devices;
    for (const auto& pair : discovered_devices_) {
        devices.push_back(pair.second.device);
    }
    return devices;
}

void DiscoveryManager::SetDeviceFoundCallback(std::function<void(const DeviceInfo&)> callback) {
    device_found_callback_ = callback;
}

void DiscoveryManager::SetDeviceLostCallback(std::function<void(std::string_view)> callback) {
    device_lost_callback_ = callback;
}

awaitable<void> DiscoveryManager::broadcaster() {
    try {
        const std::string broadcast_address = "255.255.255.255";

        const uint16_t broadcast_port = 37020;

        ip::udp::endpoint broadcast_endpoint(ip::make_address(broadcast_address), broadcast_port);

        json device_json = DeviceInfo::LocalDeviceInfo();
        std::string data = device_json.dump();

        while (broadcast_socket_.is_open()) {
            spdlog::info("start to broadcast device info every 3 seconds...");

            co_await broadcast_socket_.async_send_to(buffer(data),
                                                     broadcast_endpoint,
                                                     use_awaitable);

            co_await broadcast_timer_.async_wait(use_awaitable);
            broadcast_timer_.expires_after(std::chrono::seconds(3)); // 每 3 秒广播一次
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in broadcaster: {}", e.what());
    }
}

std::string DiscoveryManager::generateDeviceId() {
    auto now = std::chrono::system_clock::now();

    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                         .count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    int randomNum = dis(gen);

    return std::to_string(timestamp) + "_" + std::to_string(randomNum);
}
awaitable<void> DiscoveryManager::listener() {
    try {
        constexpr size_t buffer_size = 1024;
        std::array<char, buffer_size> recv_buffer;
        ip::udp::endpoint sender_endpoint;

        while (listen_socket_.is_open()) {
            size_t bytes_received = co_await listen_socket_.async_receive_from(buffer(recv_buffer),
                                                                               sender_endpoint,
                                                                               use_awaitable);

            std::string data(recv_buffer.data(), bytes_received);

            try {
                json device_json = json::parse(data);
                DeviceInfo device = device_json.get<DeviceInfo>();
                // 判断是否是自己的设备，若是则跳过后续处理
                if (device.device_id == device_id_) {
                    spdlog::info("Received message from self, skipping...");
                    // std::cout << "Received message from self, skipping..." << std::endl;
                    continue;
                }
                AddDevice(device);
                spdlog::info("Received device info from {}:{}",
                             sender_endpoint.address().to_string(),
                             sender_endpoint.port());
            } catch (const json::parse_error& e) {
                spdlog::error("Error parsing JSON data: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in listener: {}", e.what());
    }
}

awaitable<void> DiscoveryManager::cleanupDevices() {
    try {
        const auto timeout_duration = std::chrono::seconds(10); // 设备超时时间
        const auto cleanup_interval = std::chrono::seconds(5);  // 清理间隔

        while (listen_socket_.is_open()) {
            co_await cleanup_timer_.async_wait(use_awaitable);
            cleanup_timer_.expires_after(cleanup_interval);

            // std::lock_guard<std::mutex> lock(devices_mutex_);
            auto now = std::chrono::system_clock::now();
            for (auto it = discovered_devices_.begin(); it != discovered_devices_.end();) {
                if (now - it->second.last_seen > timeout_duration) {
                    std::string device_id = it->first;
                    it = discovered_devices_.erase(it);
                    if (device_lost_callback_) {
                        device_lost_callback_(device_id);
                    }
                } else {
                    ++it;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in cleanup_devices: {}", e.what());
    }
}

} // namespace lansend::core
