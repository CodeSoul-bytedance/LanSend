#include <ipc/ipc_event_stream.h>
#include <ipc/model.h>
#include <spdlog/spdlog.h>

namespace lansend {

void IpcEventStream::PostOperation(Operation&& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (operation.type == OperationType::kRespondToReceiveRequest) {
        ConfirmReceiveOperation confirm_receive_operation;
        try {
            nlohmann::from_json(operation.data, confirm_receive_operation);
            confirm_receive_operation_ = std::move(confirm_receive_operation);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse ConfirmReceiveOperation: {}", e.what());
            return;
        }
    } else if (operation.type == OperationType::kCancelReceive) {
        cancel_receive_operation_ = true;
    } else {
        active_operations_.emplace_back(std::move(operation));
    }
}

void IpcEventStream::PostOperation(const Operation& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (operation.type == OperationType::kRespondToReceiveRequest) {
        ConfirmReceiveOperation confirm_receive_operation;
        try {
            nlohmann::from_json(operation.data, confirm_receive_operation);
            confirm_receive_operation_ = std::move(confirm_receive_operation);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse ConfirmReceiveOperation: {}", e.what());
            return;
        }
    } else if (operation.type == OperationType::kCancelReceive) {
        cancel_receive_operation_ = true;
    } else {
        active_operations_.emplace_back(operation);
    }
}

void IpcEventStream::PostNotification(Notification&& notification) {
    std::lock_guard<std::mutex> lock(mutex_);
    notifications_.emplace_back(std::move(notification));
}

void IpcEventStream::PostNotification(const Notification& notification) {
    std::lock_guard<std::mutex> lock(mutex_);
    notifications_.emplace_back(notification);
}

std::optional<Operation> IpcEventStream::PollActiveOperation() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_operations_.empty()) {
        return std::nullopt;
    }
    Operation op = active_operations_.front();
    active_operations_.pop_front();
    return op;
}

std::optional<ConfirmReceiveOperation> IpcEventStream::PollConfirmReceiveOperation() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!confirm_receive_operation_) {
        return std::nullopt;
    }
    ConfirmReceiveOperation operation = *confirm_receive_operation_;
    confirm_receive_operation_ = std::nullopt;
    return operation;
}

bool IpcEventStream::PollCancelReceiveOperation() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cancel_receive_operation_) {
        return false;
    }
    cancel_receive_operation_ = false;
    return true;
}

std::optional<Notification> IpcEventStream::PollNotification() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (notifications_.empty()) {
        return std::nullopt;
    }
    Notification notification = notifications_.front();
    notifications_.pop_front();
    return notification;
}

} // namespace lansend