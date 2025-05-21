#include "core/constant/path.h"
#include "core/model/feedback.h"
#include "core/model/feedback/feedback_type.h"
#include "core/security/certificate_manager.h"
#include "core/util/config.h"
#include <ipc/ipc_backend_service.h>

namespace net = boost::asio;

namespace lansend::ipc {

IpcBackendService::IpcBackendService(boost::asio::io_context& ioc, IpcEventStream& event_stream)
    : ioc_(ioc)
    , event_stream_(event_stream)
    , cert_manager_(core::path::kCertificateDir)
    , http_client_service_(ioc, cert_manager_)
    , http_server_(ioc, cert_manager_)
    , discovery_manager_(ioc)
    , is_running_(true) {
    discovery_manager_.SetDeviceFoundCallback([this](const core::DeviceInfo& device) {
        event_stream_.PostFeedback(core::Feedback{
            .type = core::FeedbackType::kFoundDevice,
            .data = core::feedback::FoundDevice{.device_info = device},
        });
    });
    discovery_manager_.SetDeviceLostCallback([this](std::string_view device_id) {
        event_stream_.PostFeedback(core::Feedback{
            .type = core::FeedbackType::kLostDevice,
            .data = core::feedback::LostDevice{.device_id = device_id.data()},
        });
    });
    http_client_service_.SetFeedbackCallback(
        [this](core::Feedback&& feedback) { event_stream_.PostFeedback(std::move(feedback)); });
    http_server_.SetFeedbackCallback(
        [this](core::Feedback&& feedback) { event_stream_.PostFeedback(std::move(feedback)); });
    http_server_.SetReceiveWaitConditionFunc([this]() -> std::optional<std::vector<std::string>> {
        if (auto operation = event_stream_.PollConfirmReceiveOperation(); operation) {
            return operation->accepted_files;
        }
        return std::nullopt;
    });
    http_server_.SetReceiveCancelConditionFunc(
        [this]() -> bool { return event_stream_.PollCancelReceiveOperation(); });
}

void IpcBackendService::Start() {
    net::co_spawn(ioc_, start(), net::detached);
    spdlog::debug("IpcBackendService started");
}

void IpcBackendService::Stop() {
    is_running_ = false;
    spdlog::debug("IpcBackendService stopped");
}

void IpcBackendService::SetExitAppCallback(std::function<void()>&& callback) {
    exit_app_callback_ = std::move(callback);
}

net::awaitable<void> IpcBackendService::start() {
    auto executor = co_await net::this_coro::executor;
    http_server_.Start(core::settings.port);
    discovery_manager_.Start(core::settings.port);
    while (is_running_) {
        if (auto operation = event_stream_.PollActiveOperation(); operation) {
            dispatchOperation(*operation);
        } else {
            co_await net::post(executor);
        }
    }
}

void IpcBackendService::dispatchOperation(const Operation& operation) {
    switch (operation.type) {
    case OperationType::kSendFiles: {
        spdlog::debug("IpcBackendService: dispatch operation \"SendFiles\"");
        if (auto data = operation.getData<operation::SendFiles>(); data) {
            sendFiles(*data);
        } else {
            spdlog::error("IPC Error: Invalid data for operation \"SendFiles\"");
        }
        break;
    }
    case OperationType::kCancelWaitForConfirmation: {
        spdlog::debug("IpcBackendService: dispatch operation \"CancelWaitForConfirmation\"");
        if (auto data = operation.getData<operation::CancelWaitForConfirmation>(); data) {
            cancelWaitForConfirmation(data->device_id);
        } else {
            spdlog::error("IPC Error: Invalid data for operation \"CancelWaitForConfirmation\"");
        }
        break;
    }
    case OperationType::kCancelSend: {
        spdlog::debug("IpcBackendService: dispatch operation \"CancelSend\"");
        if (auto data = operation.getData<operation::CancelSend>(); data) {
            cancelSend(data->session_id);
        } else {
            spdlog::error("IPC Error: Invalid data for operation \"CancelSend\"");
        }
        break;
    }
    case OperationType::kConnectToDevice: {
        spdlog::debug("IpcBackendService: dispatch operation \"ConnectToDevice\"");
        if (auto data = operation.getData<operation::ConnectToDevice>(); data) {
            connectToDevice(data->device_id, data->pin_code);
        } else {
            spdlog::error("IPC Error: Invalid data for operation \"ConnectToDevice\"");
        }
        break;
    }
    case OperationType::kExitApp: {
        exitApp();
        break;
    }
    case OperationType::kModifySettings: {
        spdlog::debug("IpcBackendService: dispatch operation \"ModifySettings\"");
        if (auto data = operation.getData<operation::ModifySettings>(); data) {
            modifySettings(data->key, data->value);
        } else {
            spdlog::error("IPC Error: Invalid data for operation \"ModifySettings\"");
        }
        break;
    }
    case OperationType::kCancelReceive: {
        spdlog::debug("IpcBackendService: dispatchOperation \"CancelReceive\"");
        spdlog::error("IPC Error: operation \"CancelReceive\" should not be polled here");
        break;
    }
    case OperationType::kRespondToReceiveRequest: {
        spdlog::debug("IpcBackendService: dispatch operation \"RespondToReceiveRequest\"");
        spdlog::error("IPC Error: operation \"RespondToReceiveRequest\" should not be polled here");
        break;
    }
    }
}

void IpcBackendService::sendFiles(const operation::SendFiles& send_file) {
    auto device = discovery_manager_.GetDevice(send_file.device_id);
    if (!device) {
        spdlog::error("IPC Error: Device not found");
        return;
    }
    if (send_file.file_paths.empty()) {
        spdlog::error("IPC Error: No files to send");
        return;
    }
    if (send_file.file_paths.size() > 10) {
        spdlog::error("IPC Error: Too many files to send");
        return;
        std::vector<std::filesystem::path> file_paths;
        for (const auto& file_path : send_file.file_paths) {
            file_paths.emplace_back(file_path);
        }
        http_client_service_.SendFiles(device->ip_address, device->port, file_paths);
    }
}

void IpcBackendService::modifySettings(std::string_view key, nlohmann::json value) {
    try {
        if (key == "port") {
            core::settings.port = value.get<std::uint16_t>();
        } else if (key == "pin-code") {
            core::settings.pin_code = value.get<std::string>();
        } else if (key == "auto-receive") {
            core::settings.auto_receive = value.get<bool>();
        } else if (key == "save-dir") {
            core::settings.save_dir = value.get<std::string>();
        } else {
            spdlog::error("IPC Error: Invalid key for ModifySettings");
            return;
        }
    } catch (const std::exception& e) {
        spdlog::error("IPC Error: Failed to modify settings: {}", e.what());
        return;
    }
}

void IpcBackendService::cancelWaitForConfirmation(const std::string& device_id) {
    if (auto device = discovery_manager_.GetDevice(device_id); device) {
        http_client_service_.CancelWaitForConfirmation(device->ip_address, device->port);
    } else {
        spdlog::error("IPC Error: Device not found");
    }
}

void IpcBackendService::cancelSend(const std::string& session_id) {
    http_client_service_.CancelSend(session_id);
}

void IpcBackendService::connectToDevice(const std::string& device_id, std::string_view pin_code) {
    if (auto device = discovery_manager_.GetDevice(device_id); device) {
        http_client_service_.ConnectDevice(pin_code, device->ip_address, device->port);
    } else {
        spdlog::error("IPC Error: Device not found");
    }
}

void IpcBackendService::exitApp() {
    Stop();
    ioc_.stop();
    if (exit_app_callback_) {
        exit_app_callback_();
    }
}

} // namespace lansend::ipc