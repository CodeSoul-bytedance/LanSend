#include "api/pipe_communication.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include <algorithm> // For std::find_if
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <chrono>
#include <constants/path.hpp>
#include <cstdlib>    // Required for getenv
#include <ctime>      // Added for std::time_t etc.
#include <filesystem> // Required for path manipulation and directory creation
#include <fstream>    // Added for std::ofstream
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <linux/limits.h> // For PATH_MAX
#include <unistd.h>       // For readlink
#elif __APPLE__
#include <limits.h> // For PATH_MAX
#include <mach-o/dyld.h>
#endif

// 声明一些必要的处理函数
boost::asio::awaitable<nlohmann::json> handle_scan_devices(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_send_request(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_accept_transfer(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_reject_transfer(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_cancel_transfer(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_get_transfer_status(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_get_active_transfers(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_update_settings(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_open_file_location(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_delete_transfer_record(const nlohmann::json& data);

int main(int argc, char* argv[]) {
    lansend::Logger logger(
#ifdef LANSEND_DEBUG
        lansend::Logger::Level::debug,
#else
        lansend::Logger::Level::info,
#endif
        (lansend::path::kLogDir / "lansend.log").string());

    spdlog::info("LanSend backend starting...");
    // Log the intended path for verification, even if file sink might fail silently later
    spdlog::info("Attempting to log to file: {}", (lansend::path::kLogDir / "lansend.log").string());

    // 初始化配置
    lansend::init_config();
    spdlog::info("LanSend backend started, using pipe communication");

    // 解析命令行参数以获取管道名称
    std::string stdin_pipe_name;
    std::string stdout_pipe_name;
    std::vector<std::string_view> args(argv, argv + argc);

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--stdin-pipe-name" && i + 1 < args.size()) {
            stdin_pipe_name = args[i + 1];
            spdlog::info("Received stdin pipe name: {}", stdin_pipe_name);
            i++; // Skip next argument as it's the value
        } else if (args[i] == "--stdout-pipe-name" && i + 1 < args.size()) {
            stdout_pipe_name = args[i + 1];
            spdlog::info("Received stdout pipe name: {}", stdout_pipe_name);
            i++; // Skip next argument as it's the value
        }
    }

    if (stdin_pipe_name.empty() || stdout_pipe_name.empty()) {
        spdlog::critical("Pipe names not provided via command line arguments. Exiting.");
        spdlog::critical("Usage: lansend --stdin-pipe-name <name> --stdout-pipe-name <name>");
        return 1;
    }

    try {
        // 创建IO上下文
        boost::asio::io_context io_context;

        // 创建管道通信处理器，传递管道名称
        lansend::api::PipeCommunication pipe_comm(io_context, stdin_pipe_name, stdout_pipe_name);

        // 注册消息处理器
        pipe_comm.register_handler("scan_devices", handle_scan_devices);
        pipe_comm.register_handler("send_request", handle_send_request);
        pipe_comm.register_handler("accept_transfer", handle_accept_transfer);
        pipe_comm.register_handler("reject_transfer", handle_reject_transfer);
        pipe_comm.register_handler("cancel_transfer", handle_cancel_transfer);
        pipe_comm.register_handler("get_transfer_status", handle_get_transfer_status);
        pipe_comm.register_handler("get_active_transfers", handle_get_active_transfers);
        pipe_comm.register_handler("update_settings", handle_update_settings);
        pipe_comm.register_handler("open_file_location", handle_open_file_location);
        pipe_comm.register_handler("delete_transfer_record", handle_delete_transfer_record);

        // 启动管道通信
        pipe_comm.start();

        // 发送启动成功的通知
        boost::asio::co_spawn(
            io_context,
            [&pipe_comm]() -> boost::asio::awaitable<void> {
                co_await pipe_comm.send_message("backend_started",
                                                {{"version", "1.0.0"},
                                                 {"platform",
#ifdef _WIN32
                                                  "windows"
#elif defined(__APPLE__)
                                                  "macos"
#else
                                                  "linux"
#endif
                                                 }});
            },
            boost::asio::detached);

        // 保存配置
        lansend::save_config();

        // 运行IO上下文，处理消息
        io_context.run();

        spdlog::info("LanSend backend event loop stopped.");

        // 确保所有日志都写入到文件
        spdlog::info("刷新日志到文件...");
        spdlog::default_logger()->flush();

        // 强制等待一小段时间，确保异步日志队列处理完成
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        return 0;
    } catch (const std::exception& e) {
        spdlog::critical("Unhandled exception in main: {}", e.what());
        if (spdlog::default_logger()) {
            spdlog::default_logger()->flush();
        }
        // Consider if spdlog::shutdown() or a custom Logger::shutdown() is needed here
        // if the Logger instance's destructor might not run or complete.
        return 1;
    } catch (...) {
        spdlog::critical("Unhandled unknown exception in main.");
        if (spdlog::default_logger()) {
            spdlog::default_logger()->flush();
        }
        // Similarly, consider explicit logger shutdown here.
        return 1;
    }

    // spdlog::info("LanSend backend main function is returning.");
    // return 0;
}