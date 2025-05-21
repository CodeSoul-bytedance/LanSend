#pragma once

#include <nlohmann/json.hpp>

namespace lansend::core {

enum class FeedbackType {
    kError,             // 普通错误通知，包含错误信息 error 和各种错误的附加数据data（一个json对象）
    kFoundDevice,       // 发现了新的设备 （设备完整信息）
    kLostDevice,        // 失去了一个设备（设备下线，只包含设备的device_id）
    kSettings,          // 设置内容，程序启动时通知ui去显示
    kConnectedToDevice, // 成功连接到某设备 （包含设备的device_id）
    kRecipientAccepted, // 对方同意接收文件（包含对方的device_id，文件名列表表示要接收的文件）
    kRecipientDeclined, // 对方拒绝接收文件（只包含对方的device_id）
    kFileSendingProgress, // 正在发送文件时的发送进度（接收者的device_id，文件名，文件进度（百分比））
    kFileSendingCompleted, // 文件通过了整体的Hash校验，发送完成（只包含接收者的device_id和文件名）
    kAllSendingCompleted,  // 全部发送完成（只包含接收者的device_id，也可以ui层自己判断）
    kRequestReceiveFiles, // 接收到了新的传输请求（每次只能接收一个人的，则已经在接收时，处于等待的其他传输请求不会触发）
    kFileReceivingProgress,  // 正在接收时的接收进度 （包含文件名和文件进度（百分比））
    kFileReceivingCompleted, // 文件通过了整体的Hash校验，接收完成（只包含文件名）
    kAllReceivingCompleted,  // 全部接收完成（啥也没有就是通知一下，也可以ui层自己判断）
    kSendingCancelledByReceiver, // 用户作为发送者在发送文件时，接收者取消了接收（只包含接收者的device_id）
    kReceivingCancelledBySender, // 用户作为接收者在接收文件时，发送者取消了发送 （暂时不包含任何数据）
    kReceivingError,             // 接收文件发生错误（包含一条错误信息）
    kSendingError,               // 发送文件发生错误（包含一条错误信息和接收者的device_id）
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    FeedbackType,
    {
        {FeedbackType::kError, "Error"},
        {FeedbackType::kFoundDevice, "FoundDevice"},
        {FeedbackType::kLostDevice, "LostDevice"},
        {FeedbackType::kSettings, "Settings"},
        {FeedbackType::kConnectedToDevice, "ConnectedToDevice"},
        {FeedbackType::kRecipientAccepted, "RecipientAccepted"},
        {FeedbackType::kRecipientDeclined, "RecipientDeclined"},
        {FeedbackType::kFileSendingProgress, "FileSendingProgress"},
        {FeedbackType::kFileSendingCompleted, "FileSendingCompleted"},
        {FeedbackType::kAllSendingCompleted, "AllSendingCompleted"},
        {FeedbackType::kRequestReceiveFiles, "RequestReceiveFiles"},
        {FeedbackType::kFileReceivingProgress, "FileReceivingProgress"},
        {FeedbackType::kFileReceivingCompleted, "FileReceivingCompleted"},
        {FeedbackType::kAllReceivingCompleted, "AllReceivingCompleted"},
        {FeedbackType::kSendingCancelledByReceiver, "SendingCancelledByReceiver"},
        {FeedbackType::kReceivingCancelledBySender, "ReceivingCancelledBySender"},
    });

} // namespace lansend::core