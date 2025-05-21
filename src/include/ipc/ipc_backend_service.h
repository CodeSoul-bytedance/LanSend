#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <core/model/feedback/feedback.h>
#include <core/network/client/http_client_service.h>
#include <core/network/client/send_session_manager.h>
#include <core/network/discovery/discovery_manager.h>
#include <core/network/server/http_server.h>
#include <core/security/certificate_manager.h>
#include <core/util/config.h>
#include <core/util/logger.h>
#include <ipc/ipc_service.h>
#include <ipc/model/operation.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

namespace lansend::ipc {

// all basic functions of lansend intergrated
// poll events from IpcEventStream and handling them
// post feedback(notification) to IpcEventStream
class IpcBackendService {
public:
    IpcBackendService();
    ~IpcBackendService();

    // 禁止拷贝和赋值
    IpcBackendService(const IpcBackendService&) = delete;
    IpcBackendService& operator=(const IpcBackendService&) = delete;

    // 启动和停止服务
    void start(const std::string& stdin_pipe_name, const std::string& stdout_pipe_name);
    void stop();

    // 获取单例实例
    static IpcBackendService* Instance() {
        static IpcBackendService instance;
        return &instance;
    }

private:
    // 事件轮询和处理
    void poll_events();
    void handle_event(const lansend::core::Feedback& feedback);
    void handle_operation(const lansend::ipc::Operation& operation);

    // 操作处理函数
    boost::asio::awaitable<void> handle_send_file(const nlohmann::json& data);
    boost::asio::awaitable<void> handle_cancel_wait_for_confirmation(const nlohmann::json& data);
    boost::asio::awaitable<void> handle_cancel_send(const nlohmann::json& data);
    boost::asio::awaitable<void> handle_respond_to_receive_request(const nlohmann::json& data);
    boost::asio::awaitable<void> handle_cancel_receive(const nlohmann::json& data);
    boost::asio::awaitable<void> handle_modify_settings(const nlohmann::json& data);
    boost::asio::awaitable<void> handle_connect_to_device(const nlohmann::json& data);
    boost::asio::awaitable<void> handle_exit_app(const nlohmann::json& data);

    // 核心组件
    boost::asio::io_context io_context_;
    std::thread io_thread_;
    std::atomic<bool> running_{false};

    // 服务组件
    std::unique_ptr<lansend::core::DiscoveryManager> discovery_manager_;
    std::unique_ptr<lansend::core::CertificateManager> cert_manager_;
    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
    std::unique_ptr<lansend::core::HttpServer> http_server_;
    // std::unique_ptr<SendSessionManager> send_session_manager_;
    std::unique_ptr<lansend::ipc::IpcService> ipc_service_;
    std::unique_ptr<lansend::core::HttpClientService> http_client_service_;

    // 定时器
    boost::asio::steady_timer poll_timer_;

    // 配置
    lansend::core::Settings& settings_;
};

} // namespace lansend::ipc