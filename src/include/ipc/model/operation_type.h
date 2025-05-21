#pragma once

#include <nlohmann/json.hpp>

namespace lansend::ipc {

enum class OperationType {
    kSendFile,                  // 发送者要求向别人发送文件
    kCancelWaitForConfirmation, // 发送者取消等待对方确认接收
    kCancelSend,                // 发送已经开始，发送者要取消
    kRespondToReceiveRequest, // 对于当前传输请求，接收者做出了是否接收的决定（同意或拒绝，同意可以只接收一部分文件）
    kCancelReceive,   // 接收已经开始，接收者要取消
    kModifySettings,  // 修改设置
    kConnectToDevice, // 要求连接到某设备，需要提供设备id和设备的AuthCode
    kExitApp,         // 要求退出应用
};

NLOHMANN_JSON_SERIALIZE_ENUM(OperationType,
                             {
                                 {OperationType::kSendFile, "SendFile"},
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