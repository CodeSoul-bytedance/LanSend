#pragma once

#include "../api/http_server.hpp"
#include "../api/rest_api_handler.hpp"
#include "../discovery/discovery_manager.hpp"
#include "../security/certificate_manager.hpp"
#include "../transfer/transfer_manager.hpp"
#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <boost/asio.hpp>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// --- 前向声明 (如果需要) ---
class DiscoveryManager;
class TransferManager;
class CertificateManager;
class RestApiHandler; // 假设这个文件只包含前向声明
class Config;
class Logger;
namespace lansend {
namespace api {
class HttpServer;
}
} // namespace lansend

class NetworkManager {
public:
    NetworkManager(boost::asio::io_context& ioc);
    ~NetworkManager();

    // 启动服务
    void start(uint16_t port);
    void stop();

    // 设备发现相关
    void start_discovery();
    void stop_discovery();
    std::vector<DiscoveryManager::DeviceInfo> get_discovered_devices() const;

    // 文件传输相关
    boost::asio::awaitable<TransferManager::TransferResult> send_file(
        const DiscoveryManager::DeviceInfo& target, const std::filesystem::path& filepath);

    // 为Electron预留的事件通知接口
    void set_device_found_callback(std::function<void(const DiscoveryManager::DeviceInfo&)> callback);
    void set_transfer_progress_callback(
        std::function<void(const TransferManager::TransferProgress&)> callback);
    void set_transfer_complete_callback(
        std::function<void(const TransferManager::TransferResult&)> callback);

private:
    boost::asio::io_context& io_context_;
    Config& config_;
    Logger& logger_;

    std::unique_ptr<lansend::api::HttpServer> server_;
    std::unique_ptr<DiscoveryManager> discovery_manager_;
    std::unique_ptr<TransferManager> transfer_manager_;
    std::unique_ptr<CertificateManager> cert_manager_;

    // 事件回调
    std::function<void(const DiscoveryManager::DeviceInfo&)> device_found_callback_;
    std::function<void(const TransferManager::TransferProgress&)> transfer_progress_callback_;
    std::function<void(const TransferManager::TransferResult&)> transfer_complete_callback_;
};