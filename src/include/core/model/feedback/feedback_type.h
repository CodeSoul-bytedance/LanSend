#pragma once

#include <nlohmann/json.hpp>

namespace lansend::core {

enum class FeedbackType {
    kSettings,            // 设置内容，程序启动时通知ui去显示
    kFoundDevice,         // 发现了新的设备 （设备完整信息）
    kLostDevice,          // 失去了一个设备（设备下线，只包含设备的device_id）
    kConnectDeviceResult, // 连接设备的结果（包含device_id, 是否成功，失败原因）
    kNetworkError,        // 网络错误

    kRecipientAccepted, // 对方同意接收文件（包含对方的device_id，文件名列表表示要接收的文件，以及session_id）
    kRecipientDeclined,    // 对方拒绝接收文件（只包含对方的device_id）
    kFileSendingProgress,  // 正在发送文件时的发送进度（session_id，文件名，文件进度（百分比））
    kFileSendingCompleted, // 文件通过了整体的Hash校验，发送完成（只包含session_id和文件名）
    kSendSessionEnded,     // 发送会话结束的（包含session_id，是否成功，是否被对方取消，失败原因）

    kRequestReceiveFiles, // 接收到了新的传输请求（每次只能接收一个人的，则已经在接收时，处于等待的其他传输请求不会触发）
    kFileReceivingProgress,  // 正在接收时的接收进度 （包含文件名和文件进度（百分比））
    kFileReceivingCompleted, // 文件通过了整体的Hash校验，接收完成（只包含文件名）
    kReceiveSessionEnded,    // 接收会话结束
};

NLOHMANN_JSON_SERIALIZE_ENUM(FeedbackType,
                             {
                                 {FeedbackType::kSettings, "Settings"},
                                 {FeedbackType::kFoundDevice, "FoundDevice"},
                                 {FeedbackType::kLostDevice, "LostDevice"},
                                 {FeedbackType::kConnectDeviceResult, "ConnectDeviceResult"},
                                 {FeedbackType::kNetworkError, "NetworkError"},
                                 {FeedbackType::kRecipientAccepted, "RecipientAccepted"},
                                 {FeedbackType::kRecipientDeclined, "RecipientDeclined"},
                                 {FeedbackType::kFileSendingProgress, "FileSendingProgress"},
                                 {FeedbackType::kFileSendingCompleted, "FileSendingCompleted"},
                                 {FeedbackType::kSendSessionEnded, "SendSessionEnded"},
                                 {FeedbackType::kRequestReceiveFiles, "RequestReceiveFiles"},
                                 {FeedbackType::kFileReceivingProgress, "FileReceivingProgress"},
                                 {FeedbackType::kFileReceivingCompleted, "FileReceivingCompleted"},
                                 {FeedbackType::kReceiveSessionEnded, "ReceiveSessionEnded"},
                             });

} // namespace lansend::core