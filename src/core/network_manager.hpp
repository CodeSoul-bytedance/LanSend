#pragma once

#include "../api/http_server.hpp"
#include "../api/rest_api_handler.hpp"
#include "../discovery/discovery_manager.hpp"
#include "../models/device_info.hpp"
#include "../models/transfer_progress.hpp"
#include "../security/certificate_manager.hpp"
#include "../transfer/transfer_manager.hpp"
#include "../utils/config.hpp"
#include "../utils/logger.hpp"
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
class Config;
class RestApiHandler; // 假设这个文件只包含前向声明

namespace lansend {
namespace api {
class HttpServer;
}
} // namespace lansend

class NetworkManager {
public:
    NetworkManager(boost::asio::io_context& ioc, Config& config);
    ~NetworkManager();

    // 启动服务
    void start(uint16_t port);
    void stop();

    // 设备发现相关
    void start_discovery();
    void stop_discovery();
    std::vector<lansend::models::DeviceInfo> get_discovered_devices() const;
    // std::vector<TransferState> get_active_transfers();

    // 文件传输相关
    std::future<TransferResult> send_file(const lansend::models::DeviceInfo& target,
                                          const std::filesystem::path& filepath);

    // 获取传输管理器实例引用
    TransferManager& get_transfer_manager();

    // 获取当前活动的传输列表
    std::vector<TransferState> get_active_transfers();

    // 为Electron预留的事件通知接口
    void set_device_found_callback(std::function<void(const lansend::models::DeviceInfo&)> callback);
    void set_transfer_progress_callback(
        std::function<void(const lansend::models::TransferProgress&)> callback);
    void set_transfer_complete_callback(std::function<void(const TransferResult&)> callback);

private:
    Config* config_;
    boost::asio::io_context& io_context_;

    std::unique_ptr<lansend::api::HttpServer> server_;
    std::unique_ptr<DiscoveryManager> discovery_manager_;
    std::unique_ptr<TransferManager> transfer_manager_;
    std::unique_ptr<CertificateManager> cert_manager_;
    boost::asio::ssl::context ssl_context_;

    // 事件回调
    std::function<void(const lansend::models::DeviceInfo&)> device_found_callback_;
    std::function<void(const lansend::models::TransferProgress&)> transfer_progress_callback_;
    std::function<void(const TransferResult&)> transfer_complete_callback_;
};