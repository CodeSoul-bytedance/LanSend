#include <core/model/feedback.h>
#include <ipc/ipc_event_stream.h>
#include <ipc/model.h>
#include <spdlog/spdlog.h>

namespace lansend::ipc {

using Feedback = core::Feedback;

void IpcEventStream::PostOperation(Operation&& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (operation.type == OperationType::kRespondToReceiveRequest) {
        operation::ConfirmReceive confirm_receive;
        try {
            nlohmann::from_json(operation.data, confirm_receive);
            confirm_receive_operation_ = std::move(confirm_receive);
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
        operation::ConfirmReceive confirm_receive_operation;
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

void IpcEventStream::PostFeedback(Feedback&& feedback) {
    feedbacks_.emplace_back(std::move(feedback));
}

void IpcEventStream::PostFeedback(const Feedback& feedback) {
    feedbacks_.emplace_back(feedback);
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

std::optional<operation::ConfirmReceive> IpcEventStream::PollConfirmReceiveOperation() {
    if (!confirm_receive_operation_) {
        return std::nullopt;
    }
    operation::ConfirmReceive operation = *confirm_receive_operation_;
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

std::optional<Feedback> IpcEventStream::PollFeedback() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (feedbacks_.empty()) {
        return std::nullopt;
    }
    Feedback feedback = feedbacks_.front();
    feedbacks_.pop_front();
    return feedback;
}

} // namespace lansend::ipc