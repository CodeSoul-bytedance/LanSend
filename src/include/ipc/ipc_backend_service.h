#pragma once

#include "core/network/client/http_client_service.h"
#include "core/network/discovery/discovery_manager.h"
#include "core/network/server/http_server.h"
#include "core/security/certificate_manager.h"
#include "ipc_event_stream.h"
#include "model.h"
#include <boost/asio/io_context.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>

/**
 * @brief IPC后端服务类
 * 
 * @details 集成了lansend的所有基本功能。该服务负责从IpcEventStream轮询事件并处理它们，
 * 同时将反馈（通知）发送回IpcEventStream。IpcBackendService充当底层功能和IPC通信层之间的
 * 中间件，处理如文件发送、设置修改、设备连接等操作。
 * 
 * 该类管理以下组件：
 * - 证书管理器（certificate manager）
 * - 设备发现管理器（discovery manager）
 * - HTTPS客户端服务
 * - HTTPS服务器
 * 
 * @note 此类不可复制或赋值。
 */
namespace lansend::ipc {

class IpcBackendService {
public:
    IpcBackendService(boost::asio::io_context& ioc, IpcEventStream& event_stream);
    ~IpcBackendService() = default;
    IpcBackendService(const IpcBackendService&) = delete;
    IpcBackendService& operator=(const IpcBackendService&) = delete;

    void Start();
    void Stop();

    void SetExitAppCallback(std::function<void()>&& callback);

private:
    boost::asio::io_context& ioc_;
    IpcEventStream& event_stream_;
    core::CertificateManager cert_manager_;
    core::DiscoveryManager discovery_manager_;
    core::HttpClientService http_client_service_;
    core::HttpServer http_server_;
    std::function<void()> exit_app_callback_ = nullptr;
    bool is_running_{false};

    // 协程任务，开启服务
    boost::asio::awaitable<void> start();

    // 分发处理轮询到的Electron端的用户操作
    void dispatchOperation(const Operation& operation);

    // 发送文件
    void sendFiles(const operation::SendFiles& send_file);

    // 修改设置
    void modifySettings(std::string_view key, nlohmann::json value);

    // 取消等待确认
    void cancelWaitForConfirmation(const std::string& device_id);

    // 取消发送
    void cancelSend(const std::string& session_id);

    // 连接到设备
    void connectToDevice(const std::string& device_id, std::string_view pin_code);

    // 退出应用程序
    void exitApp();

    // 反馈给前端
    void feedback(const core::Feedback& feedback) { event_stream_.PostFeedback(feedback); }
};

} // namespace lansend::ipc