#pragma once

#include <nlohmann/json.hpp>

namespace lansend::ipc {

enum class OperationType {
    kSendFiles,                 // 发送者要求向别人发送文件，提供文件路径数组，对方的device_id
    kCancelWaitForConfirmation, // 发送者取消等待对方确认接收，提供对方的device_id
    kCancelSend,                // 发送已经开始，发送者要取消，提供本次传输的session_id
    kRespondToReceiveRequest, // 对于当前传输请求，接收者做出了是否接收的决定（同意或拒绝，同意可以只接收一部分文件）
    kCancelReceive,   // 接收已经开始，接收者要取消，不用提供数据
    kModifySettings,  // 修改设置
    kConnectToDevice, // 要求连接到某设备，需要提供设备id和设备的pin_code
    kExitApp,         // 要求退出应用
};

NLOHMANN_JSON_SERIALIZE_ENUM(OperationType,
                             {
                                 {OperationType::kSendFiles, "SendFiles"},
                                 {OperationType::kCancelWaitForConfirmation,
                                  "CancelWaitForConfirmation"},
                                 {OperationType::kCancelSend, "CancelSend"},
                                 {OperationType::kRespondToReceiveRequest, "ConfirmReceive"},
                                 {OperationType::kCancelReceive, "CancelReceive"},
                                 {OperationType::kModifySettings, "ModifySettings"},
                                 {OperationType::kConnectToDevice, "ConnectToDevice"},
                                 {OperationType::kExitApp, "ExitApp"},
                             });

} // namespace lansend::ipc