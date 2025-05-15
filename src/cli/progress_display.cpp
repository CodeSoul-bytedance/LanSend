#include "progress_display.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>

// 构造函数
ProgressDisplay::ProgressDisplay() {
    // 暂时为空
}

// 析构函数
ProgressDisplay::~ProgressDisplay() {
    // 暂时为空
}

// 私有方法，用于打印进度信息
void ProgressDisplay::print_progress(const lansend::models::TransferProgress& progress) {
    // 打印进度信息
    std::cout << "\rTransfer ID: " << progress.transfer_id << " | Filename: " << progress.filename
              << " | Progress: " << std::fixed << std::setprecision(2)
              << progress.percentage * 100.0 << "%"
              << " | Transferred: " << progress.bytes_transferred << " / " << progress.total_bytes
              << " bytes" << std::flush;
}

// 更新进度信息
void ProgressDisplay::update_progress(const lansend::models::TransferProgress& progress) {
    print_progress(progress);
}

// 清除进度信息
void ProgressDisplay::clear_progress() {
    std::cout << "\r" << std::string(120, ' ') << "\r" << std::flush;
}
