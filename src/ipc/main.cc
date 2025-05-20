#include "../include/core/util/logger.h"
#include "../include/ipc/ipc_backend_service.h"
#include "../include/ipc/ipc_event_stream.h"
#include <chrono>
#include <csignal>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>

// 信号处理函数
volatile sig_atomic_t g_running = 1;
void signal_handler(int signal) {
    spdlog::info("received signal: {}", signal);
    g_running = 0;
}

int main(int argc, char* argv[]) {
    // 设置日志级别
    spdlog::set_level(spdlog::level::info);
    spdlog::info("LanSend backend starting...");

    lansend::init_config();

    // 解析命令行参数，获取命名管道名称
    std::string stdin_pipe_name;
    std::string stdout_pipe_name;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--stdin-pipe-name" && i + 1 < argc) {
            stdin_pipe_name = argv[++i];
        } else if (arg == "--stdout-pipe-name" && i + 1 < argc) {
            stdout_pipe_name = argv[++i];
        }
    }

    if (stdin_pipe_name.empty() || stdout_pipe_name.empty()) {
        spdlog::error("Missing pipe names");
        spdlog::error("Usage: {} --stdin-pipe <pipe_name> --stdout-pipe <pipe_name>", argv[0]);
        return 1;
    }

    spdlog::info("Using pipe names: stdin={}, stdout={}", stdin_pipe_name, stdout_pipe_name);

    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        // 获取并启动后端服务
        auto* service = lansend::IpcBackendService::Instance();
        service->start(stdin_pipe_name, stdout_pipe_name);

        spdlog::info("LanSend backend started");

        // 主循环，等待信号或其他退出条件
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 停止服务
        spdlog::info("Stopping LanSend backend...");
        service->stop();

    } catch (const std::exception& e) {
        spdlog::critical("Backend service error: {}", e.what());
        return 1;
    }

    spdlog::info("LanSend backend stopped");
    return 0;
}
