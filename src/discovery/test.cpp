#include "discovery_manager.hpp"
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>

int main() {
    boost::asio::io_context ioc;
    DiscoveryManager manager(ioc);

    // 用于标记是否发现设备
    std::atomic<bool> device_found{false};
    // 用于标记是否检测到设备丢失
    std::atomic<bool> device_lost{false};

    // 设置设备发现回调函数
    manager.set_device_found_callback([&device_found](const lansend::models::DeviceInfo& device) {
        std::cout << "Device found: " << device.device_id << std::endl;
        device_found = true;
    });
    // 设置设备丢失回调函数
    manager.set_device_lost_callback([&device_lost](const std::string& device_id) {
        std::cout << "Device lost: " << device_id << std::endl;
        device_lost = true;
    });

    // 启动发现管理器
    manager.start(37020);

    // 启动 io_context 事件循环
    std::thread io_thread([&ioc]() {
        ioc.run();
    });

    // 等待一段时间，让监听函数有机会接收到广播的设备信息
    std::this_thread::sleep_for(std::chrono::seconds(40));

    // 停止发现管理器
    manager.stop();
    ioc.stop();

    // 等待 io_context 线程结束
    if (io_thread.joinable()) {
        io_thread.join();
    }

    // 验证是否发现设备和检测到设备丢失
    if (device_found && device_lost) {
        std::cout << "Test passed: Broadcast, listen and device cleanup functions work correctly." << std::endl;
    } else {
        std::cout << "Test failed: " << (device_found ? "Device cleanup failed." : "Device discovery failed.") << std::endl;
    }

    return 0;
}
