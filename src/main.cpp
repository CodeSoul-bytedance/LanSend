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

// Function to get the directory of the current executable - RETAINED FOR NOW, BUT NOT USED FOR LOG PATH
std::filesystem::path get_executable_directory() {
    std::filesystem::path exe_path;
#ifdef _WIN32
    wchar_t path_buf[MAX_PATH];
    GetModuleFileNameW(NULL, path_buf, MAX_PATH);
    exe_path = std::filesystem::path(path_buf);
#elif __linux__
    char path_buf[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path_buf, PATH_MAX);
    if (count != -1) {
        exe_path = std::filesystem::path(std::string(path_buf, count));
    }
#elif __APPLE__
    char path_buf[PATH_MAX];
    uint32_t size = sizeof(path_buf);
    if (_NSGetExecutablePath(path_buf, &size) == 0) {
        exe_path = std::filesystem::path(path_buf);
    } else {
        // Handle error or buffer too small, though PATH_MAX should generally be enough
        // For simplicity, returning empty path on error here.
        return {};
    }
#endif
    if (!exe_path.empty()) {
        return exe_path.parent_path();
    }
    return {}; // Return empty path on failure
}

// 声明一些必要的处理函数
boost::asio::awaitable<nlohmann::json> handle_scan_devices(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_send_request(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_accept_transfer(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_reject_transfer(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_cancel_transfer(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_get_transfer_status(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_get_active_transfers(const nlohmann::json& data);
boost::asio::awaitable<nlohmann::json> handle_update_settings(const nlohmann::json& data);

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

        // 创建信号处理器，处理Ctrl+C等信号
        // boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        // signals.async_wait([&io_context](const boost::system::error_code& ec, int signal_number) {
        //     if (ec) {
        //         // 尝试记录信号处理本身的错误，如果日志记录器仍然可用
        //         if (spdlog::default_logger()) {
        //             spdlog::error("Signal handler setup error: {}. Signal number: {}", ec.message(), signal_number);
        //         } else {
        //             std::cerr << "Signal handler setup error (logger unavailable): " << ec.message() << ". Signal number: " << signal_number << std::endl;
        //         }
        //         // 即使信号处理设置出错，也尝试停止io_context
        //         io_context.stop();
        //         return;
        //     }

        //     // 尝试记录收到信号的事件。这条日志本身是否能写入文件是一个关键测试点。
        //     if (spdlog::default_logger()) {
        //         spdlog::info("Received signal {}. Attempting direct file write and spdlog shutdown...", signal_number);
        //         spdlog::default_logger()->flush(); // 先尝试刷新spdlog队列

        //         // 实验：直接写入文件
        //         std::string log_file_path_str = (lansend::path::kLogDir / "lansend.log").string();
        //         std::ofstream direct_log_stream(log_file_path_str, std::ios::app);
        //         if (direct_log_stream.is_open()) {
        //             auto now_ts = std::chrono::system_clock::now();
        //             std::time_t now_c = std::chrono::system_clock::to_time_t(now_ts);
        //             char time_buf[100];
        //             std::string time_str = std::ctime(&now_c);
        //             time_str.pop_back(); // Remove trailing newline from ctime
        //             direct_log_stream << "[" << time_str << "] [DIRECT_WRITE] Signal " << signal_number << " received. Attempting spdlog shutdown.\n";
        //             direct_log_stream.flush();
        //             direct_log_stream.close();
        //             spdlog::info("Direct write attempt completed."); // Log if spdlog still works
        //         } else {
        //             // 如果spdlog仍然工作，尝试通过它记录直接写入失败的情况
        //             spdlog::error("Failed to open log file directly for emergency signal message: {}", log_file_path_str);
        //             // 也输出到cerr以防万一
        //             std::cerr << "EMERGENCY_WRITE_FAIL: Failed to open log file directly: " << log_file_path_str << std::endl;
        //         }

        //         spdlog::shutdown();             // 强制关闭spdlog，应确保所有队列中的日志被处理
        //     } else {
        //         std::cerr << "Default logger not available during signal " << signal_number << " processing." << std::endl;
        //     }

        //     io_context.stop(); // 请求io_context停止所有操作
        // });

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

    spdlog::info("LanSend backend main function is returning.");
    // The 'logger' object (if an instance in main) goes out of scope here,
    // its destructor should be called, which should handle final log flushing and spdlog::shutdown().
    return 0;
}