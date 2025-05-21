#include "../include/core/network/server/controller/receive_controller.h"
#include "../include/core/util/system.h"
#include "../include/ipc/ipc_service.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <core/network/client/http_client_service.h>
#include <core/network/discovery/discovery_manager.h>
#include <core/network/server/http_server.h>
#include <core/security/certificate_manager.h>
#include <core/security/open_ssl_provider.h>
#include <core/util/config.h>
#include <core/util/logger.h>
#include <filesystem>
#include <ipc/ipc_backend_service.h>
#include <ipc/ipc_event_stream.h>
#include <ipc/model/notification.h>
#include <ipc/model/notification_type.h>
#include <ipc/model/operation.h>
#include <ipc/model/operation_type.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace lansend {

IpcBackendService::IpcBackendService()
    : io_context_()
    , poll_timer_(io_context_)
    , settings_(lansend::settings) {
    spdlog::info("Initializing IpcBackendService");
}

IpcBackendService::~IpcBackendService() {
    stop();
    spdlog::info("IpcBackendService destroyed");
}

void IpcBackendService::start(const std::string& stdin_pipe_name,
                              const std::string& stdout_pipe_name) {
    if (running_) {
        spdlog::warn("IpcBackendService is running");
        return;
    }

    spdlog::info("start IpcBackendService");
    running_ = true;

    ipc_service_ = std::make_unique<IpcService>(io_context_, stdin_pipe_name, stdout_pipe_name);
    ipc_service_->start();

    // 初始化证书管理器
    // 使用配置中的元数据存储路径作为证书目录
    cert_manager_ = std::make_unique<CertificateManager>(settings_.metadataStoragePath);

    // 初始化安全上下文（生成或加载证书）
    if (!cert_manager_->InitSecurityContext()) {
        spdlog::critical("Failed to initialize security context");
        throw std::runtime_error("Failed to initialize security context");
    }

    // 初始化SSL上下文
    ssl_context_ = std::make_unique<boost::asio::ssl::context>(
        OpenSSLProvider::BuildServerContext(cert_manager_->security_context().certificate_pem,
                                            cert_manager_->security_context().private_key_pem));

    // 初始化设备发现管理器
    discovery_manager_ = std::make_unique<DiscoveryManager>(io_context_);
    discovery_manager_->SetDeviceFoundCallback([this](const lansend::models::DeviceInfo& device) {
        // 发送设备发现通知
        Notification notification;
        notification.type = NotificationType::kFoundDevice;
        notification.data = nlohmann::json{{{"device_id", device.device_id},
                                            {"alias", device.alias},
                                            {"ip", device.ip_address},
                                            {"port", device.port}}};
        IpcEventStream::Instance()->PostNotification(notification);
    });

    discovery_manager_->SetDeviceLostCallback([this](const std::string& device_id) {
        // 发送设备丢失通知
        Notification notification;
        notification.type = NotificationType::kLostDevice;
        notification.data = nlohmann::json{{"device_id", device_id}};
        IpcEventStream::Instance()->PostNotification(notification);
    });

    // 初始化HTTP服务器
    http_server_ = std::make_unique<HttpServer>(io_context_, *ssl_context_);

    // 初始化发送会话管理器
    // send_session_manager_ = std::make_unique<SendSessionManager>(io_context_, *cert_manager_);

    http_client_service_ = std::make_unique<HttpClientService>(io_context_, *cert_manager_);

    // 启动设备发现
    discovery_manager_->Start(settings_.port);

    // 启动HTTP服务器
    http_server_->start(settings_.port);

    // 启动事件轮询
    poll_events();

    // 启动IO线程
    io_thread_ = std::thread([this]() {
        try {
            io_context_.run();
        } catch (const std::exception& e) {
            spdlog::error("IO thread error: {}", e.what());
        }
    });

    spdlog::info("IpcBackendService started");

    // 发送设置通知
    Notification settings_notification;
    settings_notification.type = NotificationType::kSettings;
    settings_notification.data = nlohmann::json{{{"device_id", settings_.device_id},
                                                 {"device_name", settings_.alias},
                                                 {"port", settings_.port},
                                                 {"auth_code", settings_.authCode},
                                                 {"auto_save", settings_.autoSave},
                                                 {"save_dir", settings_.saveDir.string()},
                                                 {"https", settings_.https}}};
    IpcEventStream::Instance()->PostNotification(settings_notification);
}

void IpcBackendService::stop() {
    if (!running_) {
        return;
    }

    spdlog::info("Stopping IpcBackendService");
    running_ = false;

    // 停止设备发现
    if (discovery_manager_) {
        discovery_manager_->Stop();
    }

    // 停止HTTP服务器
    if (http_server_) {
        http_server_->stop();
    }

    // 停止IO上下文
    io_context_.stop();

    // 等待IO线程结束
    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    spdlog::info("IpcBackendService stopped");
}

void IpcBackendService::poll_events() {
    if (!running_) {
        return;
    }

    // 处理操作
    auto operation = IpcEventStream::Instance()->PollActiveOperation();
    if (operation) {
        handle_operation(*operation);
    }

    // 处理通知
    auto notification = IpcEventStream::Instance()->PollNotification();
    if (notification) {
        handle_event(*notification);
    }

    // 设置下一次轮询
    poll_timer_.expires_after(std::chrono::milliseconds(100));
    poll_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            poll_events();
        }
    });
}

void IpcBackendService::handle_event(const Notification& notification) {
    spdlog::info("Processing notification: {}", static_cast<int>(notification.type));

    // 这里可以添加对特定通知类型的处理逻辑
    switch (notification.type) {
    case NotificationType::kError:
        spdlog::error("Received error notification: {}", notification.data.dump());
        break;
    default:
        // 默认情况下，只是将通知转发给前端
        break;
    }
}

void IpcBackendService::handle_operation(const Operation& operation) {
    spdlog::info("Processing operation: {}", static_cast<int>(operation.type));

    // 根据操作类型调用相应的处理函数
    switch (operation.type) {
    case OperationType::kSendFile:
        boost::asio::co_spawn(io_context_, handle_send_file(operation.data), boost::asio::detached);
        break;
    case OperationType::kCancelWaitForConfirmation:
        boost::asio::co_spawn(io_context_,
                              handle_cancel_wait_for_confirmation(operation.data),
                              boost::asio::detached);
        break;
    case OperationType::kCancelSend:
        boost::asio::co_spawn(io_context_,
                              handle_cancel_send(operation.data),
                              boost::asio::detached);
        break;
    case OperationType::kRespondToReceiveRequest:
        boost::asio::co_spawn(io_context_,
                              handle_respond_to_receive_request(operation.data),
                              boost::asio::detached);
        break;
    case OperationType::kCancelReceive:
        boost::asio::co_spawn(io_context_,
                              handle_cancel_receive(operation.data),
                              boost::asio::detached);
        break;
    case OperationType::kModifySettings:
        boost::asio::co_spawn(io_context_,
                              handle_modify_settings(operation.data),
                              boost::asio::detached);
        break;
    case OperationType::kConnectToDevice:
        boost::asio::co_spawn(io_context_,
                              handle_connect_to_device(operation.data),
                              boost::asio::detached);
        break;
    case OperationType::kExitApp:
        boost::asio::co_spawn(io_context_, handle_exit_app(operation.data), boost::asio::detached);
        break;
    default:
        spdlog::warn("Unknown operation type: {}", static_cast<int>(operation.type));
        break;
    }
}

boost::asio::awaitable<void> IpcBackendService::handle_send_file(const nlohmann::json& data) {
    spdlog::info("Processing send file request");

    if (!data.contains("target_device") || !data.contains("files")) {
        spdlog::error("Send file request missing necessary parameters");
        co_return;
    }

    std::string target_device = data["target_device"];
    auto files = data["files"];

    // 获取目标设备信息
    auto devices = discovery_manager_->GetDevices();
    auto it = std::find_if(devices.begin(),
                           devices.end(),
                           [&target_device](const lansend::models::DeviceInfo& device) {
                               return device.device_id == target_device;
                           });

    if (it == devices.end()) {
        spdlog::error("Target device not found: {}", target_device);
        co_return;
    }

    // 准备文件路径列表
    std::vector<std::filesystem::path> file_paths;
    for (const auto& file : files) {
        if (file.contains("path")) {
            file_paths.push_back(file["path"].get<std::string>());
        }
    }

    if (file_paths.empty()) {
        spdlog::error("No valid file paths");
        co_return;
    }

    // 发送文件
    try {
        // send_session_manager_->SendFiles(it->ip_address, it->port, file_paths);

        http_client_service_->SendFiles(it->ip_address, it->port, file_paths);

        // 发送通知：已连接到设备
        Notification notification;
        notification.type = NotificationType::kConnectedToDevice;
        notification.data = nlohmann::json{{"device_id", target_device}};
        IpcEventStream::Instance()->PostNotification(notification);
    } catch (const std::exception& e) {
        spdlog::error("Failed to send file: {}", e.what());

        // 发送错误通知
        Notification notification;
        notification.type = NotificationType::kError;
        notification.data = nlohmann::json{
            {{"error", "Failed to send file"}, {"message", e.what()}, {"device_id", target_device}}};
        IpcEventStream::Instance()->PostNotification(notification);
    }

    co_return;
}

boost::asio::awaitable<void> IpcBackendService::handle_cancel_wait_for_confirmation(
    const nlohmann::json& data) {
    spdlog::info("Processing cancel wait for confirmation request");

    if (!data.contains("transfer_id")) {
        spdlog::error("Cancel wait for confirmation request missing transfer_id parameter");
        co_return;
    }

    std::string transfer_id = data["transfer_id"];

    try {
        // 取消发送会话
        http_client_service_->CancelSend(transfer_id);
        spdlog::info("Successfully cancelled wait for confirmation: {}", transfer_id);

        // 发送成功通知
        Notification notification;
        notification.type = NotificationType::kSendingCancelledByReceiver;
        notification.data = nlohmann::json{{{"transfer_id", transfer_id}}};
        IpcEventStream::Instance()->PostNotification(notification);
    } catch (const std::exception& e) {
        spdlog::error("Error cancelling wait for confirmation: {}", e.what());

        // 发送错误通知
        Notification notification;
        notification.type = NotificationType::kError;
        notification.data = nlohmann::json{
            {{"error", std::string("取消等待确认时出错：") + e.what()},
             {"transfer_id", transfer_id}}};
        IpcEventStream::Instance()->PostNotification(notification);
    }

    co_return;
}

boost::asio::awaitable<void> IpcBackendService::handle_cancel_send(const nlohmann::json& data) {
    spdlog::info("Processing cancel send request");

    if (!data.contains("transfer_id")) {
        spdlog::error("Cancel send request missing transfer_id parameter");
        co_return;
    }

    std::string transfer_id = data["transfer_id"];

    // 取消发送
    try {
        http_client_service_->CancelSend(transfer_id);
        spdlog::info("Cancel send: {}", transfer_id);
    } catch (const std::exception& e) {
        spdlog::error("Failed to cancel send: {}", e.what());
    }

    co_return;
}

boost::asio::awaitable<void> IpcBackendService::handle_respond_to_receive_request(
    const nlohmann::json& data) {
    spdlog::info("Processing respond to receive request");

    if (!data.contains("transfer_id") || !data.contains("accept")) {
        spdlog::error("Respond to receive request missing necessary parameters");
        co_return;
    }

    std::string transfer_id = data["transfer_id"];
    bool accept = data["accept"];

    try {
        // 创建确认接收操作
        ConfirmReceiveOperation confirm_operation;
        confirm_operation.accepted = accept;

        // 如果接受请求，并且包含接受的文件列表
        if (accept && data.contains("accepted_files")) {
            std::vector<std::string> accepted_files;
            for (const auto& file_id : data["accepted_files"]) {
                accepted_files.push_back(file_id.get<std::string>());
            }
            confirm_operation.accepted_files = accepted_files;
        }

        // 将确认操作发送到事件流
        IpcEventStream::Instance()->PostOperation(
            Operation{.type = OperationType::kRespondToReceiveRequest, .data = confirm_operation});

        if (accept) {
            spdlog::info("Accept receive request: {}", transfer_id);

            // 发送接受通知
            Notification notification;
            notification.type = NotificationType::kRecipientAccepted;
            notification.data = nlohmann::json{{{"transfer_id", transfer_id}}};
            IpcEventStream::Instance()->PostNotification(notification);
        } else {
            spdlog::info("Reject receive request: {}", transfer_id);

            // 发送拒绝通知
            Notification notification;
            notification.type = NotificationType::kRecipientDeclined;
            notification.data = nlohmann::json{{{"transfer_id", transfer_id}}};
            IpcEventStream::Instance()->PostNotification(notification);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error responding to receive request: {}", e.what());

        // 发送错误通知
        Notification notification;
        notification.type = NotificationType::kError;
        notification.data = nlohmann::json{
            {{"error", std::string("响应接收请求时出错：") + e.what()},
             {"transfer_id", transfer_id}}};
        IpcEventStream::Instance()->PostNotification(notification);
    }

    co_return;
}

boost::asio::awaitable<void> IpcBackendService::handle_cancel_receive(const nlohmann::json& data) {
    spdlog::info("Processing cancel receive request");

    if (!data.contains("transfer_id")) {
        spdlog::error("Cancel receive request missing transfer_id parameter");
        co_return;
    }

    std::string transfer_id = data["transfer_id"];

    try {
        // 检查HTTP服务器是否已初始化
        if (!http_server_) {
            spdlog::error("HTTP server not initialized");

            // 发送错误通知
            Notification notification;
            notification.type = NotificationType::kError;
            notification.data = nlohmann::json{
                {{"error", "无法取消接收：HTTP服务器未初始化"}, {"transfer_id", transfer_id}}};
            IpcEventStream::Instance()->PostNotification(notification);
            co_return;
        }

        // 获取接收控制器并重置为空闲状态，这将取消当前的接收会话
        auto& receive_controller = http_server_->GetReceiveController();

        // 检查当前是否有接收会话
        if (receive_controller.session_status() != lansend::ReceiveSessionStatus::kReceiving) {
            spdlog::warn("No active receive session to cancel");

            // 发送错误通知
            Notification notification;
            notification.type = NotificationType::kError;
            notification.data = nlohmann::json{
                {{"error", "没有活动的接收会话可取消"}, {"transfer_id", transfer_id}}};
            IpcEventStream::Instance()->PostNotification(notification);
            co_return;
        }

        // 重置接收控制器状态，取消接收
        receive_controller.resetToIdle();
        spdlog::info("Successfully cancelled receive session: {}", transfer_id);

        // 发送成功通知
        Notification notification;
        notification.type = NotificationType::kReceivingCancelledBySender;
        notification.data = nlohmann::json{{{"transfer_id", transfer_id}}};
        IpcEventStream::Instance()->PostNotification(notification);
    } catch (const std::exception& e) {
        spdlog::error("Error cancelling receive: {}", e.what());

        // 发送错误通知
        Notification notification;
        notification.type = NotificationType::kError;
        notification.data = nlohmann::json{
            {{"error", std::string("取消接收时出错：") + e.what()}, {"transfer_id", transfer_id}}};
        IpcEventStream::Instance()->PostNotification(notification);
    }

    co_return;
}

boost::asio::awaitable<void> IpcBackendService::handle_modify_settings(const nlohmann::json& data) {
    spdlog::info("Processing modify settings request");

    if (!data.contains("settings")) {
        spdlog::error("Modify settings request missing settings parameter");
        co_return;
    }

    auto settings_data = data["settings"];
    bool need_restart = false;

    // 更新设备名称
    if (settings_data.contains("device_name")) {
        settings_.alias = settings_data["device_name"];
    }

    // 更新端口
    if (settings_data.contains("port")) {
        uint16_t new_port = settings_data["port"];
        if (new_port != settings_.port) {
            settings_.port = new_port;
            need_restart = true;
        }
    }

    // 更新认证码
    if (settings_data.contains("auth_code")) {
        settings_.authCode = settings_data["auth_code"];
    }

    // 更新自动保存设置
    if (settings_data.contains("auto_save")) {
        settings_.autoSave = settings_data["auto_save"];
    }

    // 更新保存目录
    if (settings_data.contains("save_dir")) {
        settings_.saveDir = std::filesystem::path(settings_data["save_dir"].get<std::string>());
    }

    // 更新HTTPS设置
    if (settings_data.contains("https")) {
        bool new_https = settings_data["https"];
        if (new_https != settings_.https) {
            settings_.https = new_https;
            need_restart = true;
        }
    }

    // 保存配置
    save_config();

    co_return;
}

boost::asio::awaitable<void> IpcBackendService::handle_connect_to_device(const nlohmann::json& data) {
    spdlog::info("Processing connect to device request");

    if (!data.contains("device_id") || !data.contains("auth_code")) {
        spdlog::error("Connect to device request missing necessary parameters");
        co_return;
    }

    std::string device_id = data["device_id"];
    std::string auth_code = data["auth_code"];

    // 获取设备信息
    auto devices = discovery_manager_->GetDevices();
    auto it = std::find_if(devices.begin(),
                           devices.end(),
                           [&device_id](const lansend::models::DeviceInfo& device) {
                               return device.device_id == device_id;
                           });

    if (it == devices.end()) {
        spdlog::error("Device not found: {}", device_id);

        // 发送错误通知
        Notification notification;
        notification.type = NotificationType::kError;
        notification.data = nlohmann::json{
            {{"error", "Device not found"}, {"device_id", device_id}}};
        IpcEventStream::Instance()->PostNotification(notification);
        co_return;
    }

    // 创建HTTP客户端
    auto client = std::make_unique<HttpsClient>(io_context_, *cert_manager_);

    try {
        // 连接到目标设备
        spdlog::info("Trying to connect to device: {}:{}", it->ip_address, it->port);
        auto connected = co_await client->Connect(it->ip_address, it->port);

        if (!connected) {
            spdlog::error("Failed to connect to device: {}:{}", it->ip_address, it->port);
            Notification notification;
            notification.type = NotificationType::kError;
            notification.data = nlohmann::json{
                {{"error", "Failed to connect to device"}, {"device_id", device_id}}};
            IpcEventStream::Instance()->PostNotification(notification);
            co_return;
        }

        // 准备连接请求
        nlohmann::json connect_data = {{"auth_code", auth_code},
                                       {"device_info",
                                        {{"device_id", settings_.device_id},
                                         {"alias", settings_.alias},
                                         {"hostname", settings_.alias},
                                         {"port", settings_.port},
                                         {"os", system::OperatingSystem()},
                                         {"device_model", "PC"},
                                         {"device_type", "desktop"}}}};

        // 创建并发送连接请求
        auto req
            = client->CreateRequest<boost::beast::http::string_body>(boost::beast::http::verb::post,
                                                                     ApiRoute::kConnect.data());
        req.body() = connect_data.dump();
        req.prepare_payload();

        auto res = co_await client->SendRequest(req);

        // 处理响应
        if (res.result() == boost::beast::http::status::ok) {
            auto response_data = nlohmann::json::parse(res.body());
            bool success = response_data["success"];

            if (success) {
                spdlog::info("Connected to device: {}", device_id);

                // 将设备添加到已连接设备列表（如果需要的话）
                // connected_devices_[device_id] = std::move(client);

                // 发送通知：已连接到设备
                Notification notification;
                notification.type = NotificationType::kConnectedToDevice;
                notification.data = nlohmann::json{{"device_id", device_id},
                                                   {"device_name", it->alias}};
                IpcEventStream::Instance()->PostNotification(notification);
            } else {
                std::string message = response_data["message"];
                spdlog::error("Failed to connect to device: {}", message);

                // 发送错误通知
                Notification notification;
                notification.type = NotificationType::kError;
                notification.data = nlohmann::json{{{"error", message}, {"device_id", device_id}}};
                IpcEventStream::Instance()->PostNotification(notification);
            }
        } else {
            spdlog::error("Failed to connect to device, status code: {}", res.result_int());

            // 发送错误通知
            Notification notification;
            notification.type = NotificationType::kError;
            notification.data = nlohmann::json{
                {{"error",
                  "Failed to connect to device, status code: " + std::to_string(res.result_int())},
                 {"device_id", device_id}}};
            IpcEventStream::Instance()->PostNotification(notification);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to connect to device, exception: {}", e.what());

        // 发送错误通知
        Notification notification;
        notification.type = NotificationType::kError;
        notification.data = nlohmann::json{
            {{"error", std::string("Failed to connect to device, exception: ") + e.what()},
             {"device_id", device_id}}};
        IpcEventStream::Instance()->PostNotification(notification);
    }

    co_return;
}

boost::asio::awaitable<void> IpcBackendService::handle_exit_app(const nlohmann::json& data) {
    spdlog::info("Processing exit app request");

    // 停止服务
    stop();

    // 通知系统退出
    // 这里可以添加一些清理工作或通知其他组件

    co_return;
}

} // namespace lansend