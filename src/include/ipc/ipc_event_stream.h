#pragma once

#include <deque>
#include <ipc/model.h>
#include <mutex>
#include <nlohmann/json.hpp>

namespace lansend {

class IpcEventStream {
public:
    void PostOperation(Operation&& operation);
    void PostOperation(const Operation& operation);
    void PostNotification(Notification&& notification);
    void PostNotification(const Notification& notification);

    std::optional<Operation> PollActiveOperation();
    std::optional<ConfirmReceiveOperation> PollConfirmReceiveOperation();
    bool PollCancelReceiveOperation();
    std::optional<Notification> PollNotification();

    static IpcEventStream* Instance() {
        static IpcEventStream instance;
        return &instance;
    }

private:
    mutable std::mutex mutex_;
    // "common" and "send" operations that will be polling actively
    std::deque<Operation> active_operations_;

    // for confirm receive operations polling in ReceiveController
    std::optional<ConfirmReceiveOperation> confirm_receive_operation_;

    // for cancel receive operations polling in ReceiveController
    bool cancel_receive_operation_{false};

    // for notifications
    std::deque<Notification> notifications_;
};

} // namespace lansend