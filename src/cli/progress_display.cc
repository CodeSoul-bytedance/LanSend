#include <cli/progress_display.h>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lansend::cli {

// 构造函数
ProgressDisplay::ProgressDisplay() {
    // 暂时为空
}

// 析构函数
ProgressDisplay::~ProgressDisplay() {
    // 暂时为空
}

// 私有方法，用于打印进度信息
void ProgressDisplay::printProgress(const TransferProgress& progress) {
    // 打印进度信息
    std::cout << "\rTransfer ID: " << progress.transfer_id << " | Filename: " << progress.filename
              << " | Progress: " << std::fixed << std::setprecision(2)
              << progress.percentage * 100.0 << "%"
              << " | Transferred: " << progress.bytes_transferred << " / " << progress.total_bytes
              << " bytes" << std::flush;
}

// 更新进度信息
void ProgressDisplay::UpdateProgress(const TransferProgress& progress) {
    printProgress(progress);
}

// 清除进度信息
void ProgressDisplay::ClearProgress() {
    std::cout << "\r" << std::string(120, ' ') << "\r" << std::flush;
}

} // namespace lansend::cli
