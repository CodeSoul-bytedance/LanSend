#pragma once

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

// 前向声明
class CertificateManager;
class DiscoveryManager;
class TransferManager;
class Config;

namespace lansend {
namespace models {
struct DeviceInfo;
struct TransferProgress;
} // namespace models

class HttpServer;
} // namespace lansend

struct TransferResult {
    bool success;
    std::string error_message;
    uint64_t transfer_id;
    std::chrono::system_clock::time_point end_time;
};

struct TransferState {
    uint64_t id;
    std::string target_device_id;
    std::string target_device_name;
    std::string file_path;
    uint64_t file_size;
    uint64_t bytes_transferred;
    bool completed;
    bool success;
    std::string error_message;
};

namespace lansend::ipc {

// all basic functions of lansend integrated
// poll events from IpcEventStream and handling them
// post feedback(notification) to IpcEventStream
class IpcBackendService {
public:
    // 构造函数和析构函数
    IpcBackendService(boost::asio::io_context& ioc, Config& config);
    ~IpcBackendService();

    // 网络管理功能
    void start(uint16_t port);
    void stop();
    
    // 设备发现功能
    void start_discovery();
    void stop_discovery();
    std::vector<lansend::models::DeviceInfo> get_discovered_devices() const;
    
    // 文件传输功能
    std::future<TransferResult> send_file(const lansend::models::DeviceInfo& target,
                                         const std::filesystem::path& filepath);
    std::vector<TransferState> get_active_transfers();
    
    // 回调设置
    void set_device_found_callback(std::function<void(const lansend::models::DeviceInfo&)> callback);
    void set_transfer_progress_callback(std::function<void(const lansend::models::TransferProgress&)> callback);
    void set_transfer_complete_callback(std::function<void(const TransferResult&)> callback);
    
    // 获取传输管理器引用
    TransferManager& get_transfer_manager();

private:
    boost::asio::io_context& io_context_;
    Config* config_;
    
    // 证书管理
    std::unique_ptr<CertificateManager> cert_manager_;
    boost::asio::ssl::context ssl_context_;
    
    // 服务组件
    std::unique_ptr<lansend::HttpServer> server_;
    std::unique_ptr<DiscoveryManager> discovery_manager_;
    std::unique_ptr<TransferManager> transfer_manager_;
    
    // 回调函数
    std::function<void(const lansend::models::DeviceInfo&)> device_found_callback_;
    std::function<void(const lansend::models::TransferProgress&)> transfer_progress_callback_;
    std::function<void(const TransferResult&)> transfer_complete_callback_;
};

} // namespace lansend::ipc