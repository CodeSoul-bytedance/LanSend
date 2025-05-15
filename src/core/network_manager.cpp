#include "network_manager.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <filesystem>
#include <future>
#include <iostream>
#include <spdlog/spdlog.h>

NetworkManager::NetworkManager(boost::asio::io_context& ioc, Config& config)
    : io_context_(ioc)
    , config_(&config)
    , cert_manager_(std::make_unique<CertificateManager>(std::filesystem::path("certificates")))
    , ssl_context_(boost::asio::ssl::context::tlsv12_server)
    , server_(std::make_unique<lansend::api::HttpServer>(ioc, ssl_context_))
    , discovery_manager_(std::make_unique<DiscoveryManager>(ioc))
    , transfer_manager_(std::make_unique<TransferManager>(ioc, lansend::settings)) {
    // 配置SSL上下文
    ssl_context_.set_options(
        boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::single_dh_use);

    // 使用证书和私钥
    ssl_context_.use_certificate(boost::asio::buffer(
                                     cert_manager_->security_context().certificate_pem),
                                 boost::asio::ssl::context::pem);
    ssl_context_.use_private_key(boost::asio::buffer(
                                     cert_manager_->security_context().private_key_pem),
                                 boost::asio::ssl::context::pem);

    // Initialize callbacks to nullptr
    device_found_callback_ = nullptr;
    transfer_progress_callback_ = nullptr;
    transfer_complete_callback_ = nullptr;
}

NetworkManager::~NetworkManager() {
    stop();
}

void NetworkManager::start(uint16_t port) {
    spdlog::info("Starting NetworkManager...");

    // Generate self-signed certificate (or load if exists)
    cert_manager_->init_security_context();

    server_->start(port);
    discovery_manager_->start(port);
    spdlog::info("NetworkManager started.");
}

void NetworkManager::stop() {
    spdlog::info("Stopping NetworkManager...");
    server_->stop();
    discovery_manager_->stop();
    spdlog::info("NetworkManager stopped.");
}

void NetworkManager::start_discovery() {
    if (discovery_manager_) {
        // 使用固定端口号替代config_->settings.port
        discovery_manager_->start(56789); // 使用默认端口作为临时解决方案
    }
}

void NetworkManager::stop_discovery() {
    if (discovery_manager_) {
        discovery_manager_->stop();
    }
}

std::vector<lansend::models::DeviceInfo> NetworkManager::get_discovered_devices() const {
    if (discovery_manager_) {
        return discovery_manager_->get_devices();
    }
    return {};
}

std::future<TransferResult> NetworkManager::send_file(const lansend::models::DeviceInfo& target,
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

                    // 手动执行协程并等待结果
                    // 注意：这里需要根据 boost::asio 的协程实现来适配具体的调用方式
                    // 推荐方法 1: 在 io_context 上运行协程直到完成，然后获取结果
                    TransferResult result;
                    // 假设有一个工具方法可以将 awaitable 变成同步结果
                    // 比如 result = lansend::utils::run_awaitable(io_context_, transfer_awaitable);

                    // 推荐方法 2: 创建一个新的执行器来运行协程，这样做可能会阻塞当前线程，但在 std::async 中这是可接受的
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
                                SPDLOG_ERROR("异常发生在传输协程中: {}", e.what());
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
                    SPDLOG_ERROR("异常发生在传输管理中: {}", e.what());
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
        result.error_message = "Transfer manager not initialized.";
        result.transfer_id = 0;
        result.end_time = std::chrono::system_clock::now();
        return result;
    });
}

void NetworkManager::set_device_found_callback(
    std::function<void(const lansend::models::DeviceInfo&)> callback) {
    device_found_callback_ = callback;
    if (discovery_manager_) {
        discovery_manager_->set_device_found_callback(callback);
    }
}

void NetworkManager::set_transfer_progress_callback(
    std::function<void(const lansend::models::TransferProgress&)> callback) {
    transfer_progress_callback_ = callback;
}

void NetworkManager::set_transfer_complete_callback(
    std::function<void(const TransferResult&)> callback) {
    transfer_complete_callback_ = callback;
}

TransferManager& NetworkManager::get_transfer_manager() {
    return *transfer_manager_;
}

std::vector<TransferState> NetworkManager::get_active_transfers() {
    return transfer_manager_->get_active_transfers();
}