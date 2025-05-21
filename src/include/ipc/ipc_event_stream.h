#pragma once

#include <core/model/feedback.h>
#include <deque>
#include <ipc/model.h>
#include <mutex>
#include <nlohmann/json.hpp>

namespace lansend::ipc {

class IpcEventStream {
    using Feedback = core::Feedback;

public:
    void PostOperation(Operation&& operation);
    void PostOperation(const Operation& operation);
    void PostFeedback(Feedback&& feedback);
    void PostFeedback(const Feedback& feedback);

    std::optional<Operation> PollActiveOperation();
    std::optional<operation::ConfirmReceive> PollConfirmReceiveOperation();
    bool PollCancelReceiveOperation();
    std::optional<Feedback> PollFeedback();

    static IpcEventStream* Instance() {
        static IpcEventStream instance;
        return &instance;
    }

private:
    mutable std::mutex mutex_;
    // "common" and "send" operations that will be polling actively
    std::deque<Operation> active_operations_;

    // for confirm receive operations polling in ReceiveController
    std::optional<operation::ConfirmReceive> confirm_receive_operation_;

    // for cancel receive operations polling in ReceiveController
    bool cancel_receive_operation_{false};

    // for notifications
    std::deque<Feedback> feedbacks_;
};

} // namespace lansend::ipc