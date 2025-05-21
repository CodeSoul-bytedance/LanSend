#pragma once

namespace lansend::core {

enum class SessionStatus {
    kIdle,                // 空闲状态
    kWaiting,             // 等待接收方确认
    kDeclined,            // 接收方拒绝
    kSending,             // 发送中
    kCompleted,           // 发送完成
    kFailed,              // 发送失败
    kCancelledBySender,   // 发送方取消
    kCancelledByReceiver, // 接收方取消
};

}
