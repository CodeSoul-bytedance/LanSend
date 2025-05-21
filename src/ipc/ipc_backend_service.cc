#include <ipc/ipc_backend_service.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <filesystem>
#include <future>
#include <iostream>
#include <spdlog/spdlog.h>

// 包含必要的头文件
#include <core/network/discovery_manager.hpp> // 根据实际路径调整
#include <core/network/transfer_manager.hpp>  // 根据实际路径调整
#include <core/security/certificate_manager.hpp> // 根据实际路径调整
#include <core/network/http_server.hpp>       // 根据实际路径调整
#include <core/config.hpp>                    // 根据实际路径调整

using namespace lansend::ipc;

IpcBackendService::IpcBackendService(boost::asio::io_context& ioc, Config& config)
    : io_context_(ioc)
    , config_(&config)
    , cert_manager_(std::make_unique<CertificateManager>())
    , ssl_context_(boost::asio::ssl::context::tlsv12_server)
    , server_(std::make_unique<lansend::HttpServer>(ioc, ssl_context_))
    , discovery_manager_(std::make_unique<DiscoveryManager>(ioc))
    , transfer_manager_(std::make_unique<TransferManager>(ioc, config.get_settings())) {
    
    // 配置SSL上下文
    ssl_context_.set_options(
        boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::single_dh_use);

    // 初始化回调为nullptr
    device_found_callback_ = nullptr;
    transfer_progress_callback_ = nullptr;
    transfer_complete_callback_ = nullptr;
}

IpcBackendService::~IpcBackendService() {
    stop();
}

void IpcBackendService::start(uint16_t port) {
    spdlog::info("Starting IpcBackendService...");

    // 使用证书路径
    std::filesystem::path cert_path = config_->get_certificate_path() / "cert.pem";
    std::filesystem::path key_path = config_->get_certificate_path() / "key.pem";
    
    // 检查证书是否存在，不存在则生成
    if (!std::filesystem::exists(cert_path) || !std::filesystem::exists(key_path)) {
        spdlog::info("Generating self-signed certificate...");
        std::filesystem::create_directories(config_->get_certificate_path());
        if (!cert_manager_->generate_self_signed_certificate(cert_path, key_path)) {
            spdlog::error("Failed to generate self-signed certificate");
            return;
        }
    } else {
        // 加载现有证书
        spdlog::info("Loading existing certificate...");
        if (!cert_manager_->load_certificate(cert_path, key_path)) {
            spdlog::error("Failed to load certificate");
            return;
        }
    }

    // 获取SSL上下文
    ssl_context_ = cert_manager_->get_ssl_context();

    // 启动服务
    server_->start(port);
    discovery_manager_->start(port);
    spdlog::info("IpcBackendService started on port {}", port);
}

void IpcBackendService::stop() {
    spdlog::info("Stopping IpcBackendService...");
    server_->stop();
    discovery_manager_->stop();
    spdlog::info("IpcBackendService stopped.");
}

void IpcBackendService::start_discovery() {
    if (discovery_manager_) {
        // 使用配置中的端口或默认端口
        uint16_t discovery_port = config_->get_discovery_port();
        if (discovery_port == 0) {
            discovery_port = 56789; // 默认端口
        }
        discovery_manager_->start(discovery_port);
    }
}

void IpcBackendService::stop_discovery() {
    if (discovery_manager_) {
        discovery_manager_->stop();
    }
}

std::vector<lansend::models::DeviceInfo> IpcBackendService::get_discovered_devices() const {
    if (discovery_manager_) {
        return discovery_manager_->get_devices();
    }
    return {};
}

std::future<TransferResult> IpcBackendService::send_file(const lansend::models::DeviceInfo& target,
                                                       const std::filesystem::path& filepath) {
    return std::async(std::launch::async, [this, target, filepath]() -> TransferResult {
        if (transfer_manager_) {
            // 创建一个承诺和对应的期望，以同步等待协程完成
            std::promise<TransferResult> promise;
            std::future<TransferResult> future = promise.get_future();

            // 使用 boost::asio 协程执行模式
            auto run_transfer = [this, &promise, &target, &filepath]() {
                try {
                    // 运行传输协程并获取结果
                    auto transfer_awaitable = transfer_manager_->start_transfer(target, filepath);

                    // 创建一个新的执行器来运行协程
                    boost::asio::io_context local_io;
                    bool completed = false;
                    TransferResult transfer_result;

                    // 创建一个新的协程执行器
                    boost::asio::co_spawn(
                        local_io,
                        [&transfer_awaitable,
                         &completed,
                         &transfer_result]() -> boost::asio::awaitable<void> {
                            try {
                                // 执行传输协程
                                transfer_result = co_await std::move(transfer_awaitable);
                                completed = true;
                            } catch (const std::exception& e) {
                                // 处理协程执行中的异常
                                spdlog::error("异常发生在传输协程中: {}", e.what());
                                TransferResult error_result;
                                error_result.success = false;
                                error_result.error_message = std::string("传输异常: ") + e.what();
                                error_result.transfer_id = 0;
                                error_result.end_time = std::chrono::system_clock::now();
                                transfer_result = std::move(error_result);
                                completed = true;
                            }
                        },
                        boost::asio::detached);

                    // 运行本地 IO 上下文直到协程完成
                    while (!completed) {
                        local_io.run_one();
                    }

                    // 设置结果到 promise
                    promise.set_value(std::move(transfer_result));
                } catch (const std::exception& e) {
                    // 处理其他异常
                    spdlog::error("异常发生在传输管理中: {}", e.what());
                    TransferResult error_result;
                    error_result.success = false;
                    error_result.error_message = std::string("传输过程异常: ") + e.what();
                    error_result.transfer_id = 0;
                    error_result.end_time = std::chrono::system_clock::now();
                    promise.set_value(std::move(error_result));
                }
            };

            // 执行传输操作
            run_transfer();

            // 等待协程执行完毕
            return future.get();
        }
        TransferResult result;
        result.success = false;
        result.error_message = "传输管理器未初始化";
        result.transfer_id = 0;
        result.end_time = std::chrono::system_clock::now();
        return result;
    });
}

void IpcBackendService::set_device_found_callback(
    std::function<void(const lansend::models::DeviceInfo&)> callback) {
    device_found_callback_ = callback;
    if (discovery_manager_) {
        discovery_manager_->set_device_found_callback(callback);
    }
}

void IpcBackendService::set_transfer_progress_callback(
    std::function<void(const lansend::models::TransferProgress&)> callback) {
    transfer_progress_callback_ = callback;
    if (transfer_manager_) {
        transfer_manager_->set_progress_callback(callback);
    }
}

void IpcBackendService::set_transfer_complete_callback(
    std::function<void(const TransferResult&)> callback) {
    transfer_complete_callback_ = callback;
    if (transfer_manager_) {
        transfer_manager_->set_complete_callback(callback);
    }
}

TransferManager& IpcBackendService::get_transfer_manager() {
    return *transfer_manager_;
}

std::vector<TransferState> IpcBackendService::get_active_transfers() {
    if (transfer_manager_) {
        return transfer_manager_->get_active_transfers();
    }
    return {};
}