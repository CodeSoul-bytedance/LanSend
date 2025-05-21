#include "../include/ipc/ipc_service.h"
#include "../include/core/util/logger.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/endian/conversion.hpp>
#include <cstdint>
#include <iostream>
#include <system_error>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <ipc/ipc_event_stream.h>
#include <mutex>

namespace lansend::ipc {
IpcService::IpcService(boost::asio::io_context& io_context,
                       IpcEventStream& event_stream,
                       const std::string& stdin_pipe_name_str,
                       const std::string& stdout_pipe_name_str)
    : io_context_(io_context)
    , event_stream_(event_stream)
    ,
#ifdef _WIN32
    input_(io_context)
    , output_(io_context)
    ,
#else
    input_(io_context, ::dup(STDIN_FILENO))
    , output_(io_context, ::dup(STDOUT_FILENO))
    ,
#endif
    running_(false) {

#ifdef _WIN32
    // 将 std::string 转换为 std::wstring 以用于 CreateFileW
    std::wstring stdin_pipe_name_w(stdin_pipe_name_str.begin(), stdin_pipe_name_str.end());
    std::wstring stdout_pipe_name_w(stdout_pipe_name_str.begin(), stdout_pipe_name_str.end());

    spdlog::debug("Windows: Connecting to stdin pipe: {}", stdin_pipe_name_str);
    HANDLE hStdIn = CreateFileW(stdin_pipe_name_w.c_str(),
                                GENERIC_READ,         // 后端从这个管道读取 (Node.js 写入)
                                0,                    // 不共享
                                NULL,                 //默认安全属性
                                OPEN_EXISTING,        // 打开已存在的管道
                                FILE_FLAG_OVERLAPPED, // 需要异步I/O
                                NULL                  // 无模板文件
    );

    if (hStdIn == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        spdlog::error("Windows: Failed to connect to stdin pipe '{}'. CreateFileW error: {}. Is "
                      "the Electron main process running and listening?",
                      stdin_pipe_name_str,
                      error);
        throw std::runtime_error("Failed to connect to stdin pipe. Error: " + std::to_string(error));
    }
    spdlog::debug("Windows: Connected to stdin pipe. Handle: {}", (void*) hStdIn);

    spdlog::debug("Windows: Connecting to stdout pipe: {}", stdout_pipe_name_str);
    HANDLE hStdOut = CreateFileW(stdout_pipe_name_w.c_str(),
                                 GENERIC_WRITE, // 后端向这个管道写入 (Node.js 读取)
                                 0,             // 不共享
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_FLAG_OVERLAPPED,
                                 NULL);

    if (hStdOut == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        spdlog::error("Windows: Failed to connect to stdout pipe '{}'. CreateFileW error: {}. Is "
                      "the Electron main process running and listening?",
                      stdout_pipe_name_str,
                      error);
        CloseHandle(hStdIn); // 清理已打开的句柄
        throw std::runtime_error("Failed to connect to stdout pipe. Error: "
                                 + std::to_string(error));
    }
    spdlog::debug("Windows: Connected to stdout pipe. Handle: {}", (void*) hStdOut);

    boost::system::error_code ec;
    spdlog::debug("Windows: Assigning hStdIn ({}) to input_ stream_handle...", (void*) hStdIn);
    input_.assign(hStdIn, ec);
    if (ec) {
        spdlog::error("Windows: input_.assign failed for hStdIn ({}). Boost.system error code: {}, "
                      "message: {}",
                      (void*) hStdIn,
                      ec.value(),
                      ec.message());
        CloseHandle(hStdIn); // 清理
        throw std::system_error(ec, "Failed to assign standard input handle");
    }
    spdlog::debug("Windows: input_.assign succeeded.");

    spdlog::debug("Windows: Assigning hStdOut ({}) to output_ stream_handle...", (void*) hStdOut);
    output_.assign(hStdOut, ec);
    if (ec) {
        spdlog::error("Windows: output_.assign failed for hStdOut ({}). Boost.system error code: "
                      "{}, message: {}",
                      (void*) hStdOut,
                      ec.value(),
                      ec.message());
        CloseHandle(hStdIn);  // 清理
        CloseHandle(hStdOut); // 清理
        throw std::system_error(ec, "Failed to assign standard output handle");
    }
    spdlog::debug("Windows: output_.assign succeeded.");
#endif
}

void IpcService::start() {
    if (running_) {
        return;
    }

    running_ = true;

    spdlog::info("Pipe communication started");
    boost::asio::co_spawn(
        io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await send_message("backend_started",
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

    // 启动读取消息的协程
    boost::asio::co_spawn(io_context_, read_message_loop(), [](std::exception_ptr e) {
        if (e) {
            try {
                std::rethrow_exception(e);
            } catch (const std::exception& ex) {
                spdlog::error("Pipe communication exception: {}", ex.what());
            }
        }
    });

    boost::asio::co_spawn(io_context_, read_event_stream_loop(), [](std::exception_ptr e) {
        if (e) {
            try {
                std::rethrow_exception(e);
            } catch (const std::exception& ex) {
                spdlog::error("Pipe communication exception: {}", ex.what());
            }
        }
    });
}

void IpcService::register_handler(const std::string& message_type, MessageHandler handler) {
    handlers_[message_type] = std::move(handler);
}

boost::asio::awaitable<void> IpcService::send_message(const std::string& type,
                                                      const nlohmann::json& data) {
    // 使用互斥锁确保对管道的写入是同步的
    std::unique_lock<std::mutex> lock(write_mutex_);

    try {
        // 准备消息
        nlohmann::json message = {{"feedback", type},
                                  {"data", data},
                                  {"timestamp",
                                   std::chrono::system_clock::now().time_since_epoch().count()}};

        std::string message_str = message.dump();

        spdlog::debug("Sending message: {}", message_str);

        // 写入消息长度（4字节）
        uint32_t length = static_cast<uint32_t>(message_str.size());
        uint32_t length_be = boost::endian::native_to_big(length);

        // 发送长度
        co_await boost::asio::async_write(output_,
                                          boost::asio::buffer(&length_be, sizeof(length_be)),
                                          boost::asio::use_awaitable);

        // 发送消息内容
        co_await boost::asio::async_write(output_,
                                          boost::asio::buffer(message_str),
                                          boost::asio::use_awaitable);

        // 确保输出刷新
        std::cout.flush();

        spdlog::debug("Message sent successfully: {}", type);
    } catch (const std::exception& e) {
        spdlog::error("Failed to send message: {}", e.what());
    }
}

boost::asio::awaitable<void> IpcService::read_message_loop() {
    spdlog::info("Starting read message loop");
    std::vector<char> length_buffer(4);
    std::vector<char> message_buffer;

    running_ = true;

    while (running_) {
        // Flags for outer try-catch (main loop)
        bool outer_catch_requires_delay = false;
        bool break_loop_due_to_outer_pipe_error = false;

        try {
            // 读取消息长度（4字节）
            std::size_t n = co_await boost::asio::async_read(input_,
                                                             boost::asio::buffer(length_buffer),
                                                             boost::asio::use_awaitable);

            if (n != 4) {
                spdlog::error("Failed to read message length: read {} bytes, expected 4 bytes", n);
                // 添加短暂暂停，避免在错误情况下的紧密循环
                co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                   std::chrono::milliseconds(100))
                    .async_wait(boost::asio::use_awaitable);
                continue;
            }

            // 解析消息长度（大端序）
            uint32_t length = 0;
            for (int i = 0; i < 4; ++i) {
                length = (length << 8) | static_cast<unsigned char>(length_buffer[i]);
            }

            // 检查消息长度是否合理
            if (length > 10 * 1024 * 1024) { // 10MB 限制
                spdlog::error("Message too large: {} bytes", length);
                // 添加短暂暂停
                co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                   std::chrono::milliseconds(100))
                    .async_wait(boost::asio::use_awaitable);
                continue;
            }

            // 调整缓冲区大小
            bool resize_failed = false;
            try {
                message_buffer.resize(length);
            } catch (const std::exception& e) {
                spdlog::error("Failed to resize message buffer to {} bytes: {}", length, e.what());
                resize_failed = true;
            }

            if (resize_failed) {
                // 添加短暂暂停
                co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                   std::chrono::milliseconds(100))
                    .async_wait(boost::asio::use_awaitable);
                continue;
            }

            // 读取消息内容
            bool read_content_error_requires_delay = false;
            bool break_loop_due_to_pipe_error = false;
            try {
                n = co_await boost::asio::async_read(input_,
                                                     boost::asio::buffer(message_buffer),
                                                     boost::asio::use_awaitable);
            } catch (const boost::system::system_error& e) {
                // 特别处理 Boost.Asio 系统错误
                spdlog::error(
                    "Boost.Asio system error while reading message content: {} (code: {})",
                    e.what(),
                    e.code().value());

                // 检查是否是管道关闭或断开连接
                if (e.code() == boost::asio::error::eof
                    || e.code() == boost::asio::error::connection_reset
                    || e.code() == boost::asio::error::connection_aborted
                    || e.code() == boost::asio::error::broken_pipe) {
                    spdlog::info("Pipe closed or connection reset, exiting read loop");
                    running_ = false;
                    break_loop_due_to_pipe_error = true;
                } else {
                    read_content_error_requires_delay = true;
                }
            }

            if (break_loop_due_to_pipe_error) {
                break;
            }

            if (read_content_error_requires_delay) {
                co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                   std::chrono::milliseconds(100))
                    .async_wait(boost::asio::use_awaitable);
                continue;
            }

            if (n != length) {
                spdlog::error("Failed to read message content: read {} bytes, expected {} bytes",
                              n,
                              length);
                // 添加短暂暂停
                co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                   std::chrono::milliseconds(100))
                    .async_wait(boost::asio::use_awaitable);
                continue;
            }

            // 处理消息
            try {
                std::string message_str(message_buffer.begin(), message_buffer.end());
                co_await handle_message(message_str);
            } catch (const std::exception& e) {
                spdlog::error("Error handling message: {}", e.what());
            }

        } catch (const boost::system::system_error& e) {
            // 捕获其他 Boost.Asio 系统错误
            spdlog::error("Boost.Asio system error in read_message_loop: {} (code: {})",
                          e.what(),
                          e.code().value());

            // 检查是否是管道关闭或断开连接
            if (e.code() == boost::asio::error::eof
                || e.code() == boost::asio::error::connection_reset
                || e.code() == boost::asio::error::connection_aborted
                || e.code() == boost::asio::error::broken_pipe) {
                spdlog::info(
                    "Pipe closed or connection reset, exiting read loop due to general error");
                running_ = false;
                break_loop_due_to_outer_pipe_error = true;
            } else {
                // 其他类型的错误，标记需要短暂暂停
                outer_catch_requires_delay = true;
            }
        } catch (const std::exception& e) {
            spdlog::error("Standard exception in read_message_loop: {}", e.what());
            // 假设标准异常也可能导致需要延迟以避免紧密错误循环
            outer_catch_requires_delay = true;
        }

        // 处理来自外部 catch 块的标志
        if (break_loop_due_to_outer_pipe_error) {
            break; // 退出 while 循环
        }
        if (outer_catch_requires_delay) {
            // 添加短暂暂停
            co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                               std::chrono::milliseconds(100))
                .async_wait(boost::asio::use_awaitable);
            continue; // 继续下一次 while 循环迭代
        }
    }

    spdlog::info("Exiting read message loop");
}

boost::asio::awaitable<void> IpcService::read_event_stream_loop() {
    spdlog::info("Starting read event stream loop");

    while (running_) {
        try {
            // 添加短暂延迟，避免CPU占用过高
            auto executor = co_await boost::asio::this_coro::executor;
            co_await boost::asio::steady_timer(executor, std::chrono::milliseconds(10))
                .async_wait(boost::asio::use_awaitable);

            auto feedback = event_stream_.PollFeedback();
            if (!feedback) {
                continue;
            }

            // 直接使用枚举类型对应的字符串，而不是序列化枚举值
            std::string feedback_type;
            nlohmann::json j = feedback->type;
            j.get_to(feedback_type);

            nlohmann::json data = feedback->data;

            // 获取通知类型名称
            spdlog::debug("Processing feedback: {}", feedback_type);

            // 尝试发送通知
            bool send_success = false;
            try {
                co_await send_message(feedback_type, data);
                send_success = true;
            } catch (const std::exception& send_ex) {
                spdlog::error("Error sending feedback: {}", send_ex.what());
                // 在catch块中不能使用co_await，所以只记录错误
            }

            // 如果发送失败，添加延迟
            if (!send_success) {
                auto executor = co_await boost::asio::this_coro::executor;
                co_await boost::asio::steady_timer(executor, std::chrono::milliseconds(100))
                    .async_wait(boost::asio::use_awaitable);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error reading event stream: {}", e.what());
            // 在catch块外添加延迟
        }

        // 每次循环后添加短暂延迟，无论是否出错
        auto executor = co_await boost::asio::this_coro::executor;
        co_await boost::asio::steady_timer(executor, std::chrono::milliseconds(100))
            .async_wait(boost::asio::use_awaitable);
    }
    spdlog::info("Exiting read event stream loop");
}

boost::asio::awaitable<void> IpcService::handle_message(const std::string& message_str) {
    // 在try块之前声明这些变量，以便在catch块之后仍可使用
    bool has_error = false;
    std::string error_message;

    try {
        // 解析JSON消息
        nlohmann::json message = nlohmann::json::parse(message_str);

        // 获取消息类型
        if (!message.contains("operation") || !message["operation"].is_string()) {
            spdlog::error("Invalid message format: missing operation field");
            co_return;
        }

        event_stream_.PostOperation(Operation(message["operation"], message["data"]));

    } catch (const std::exception& e) {
        spdlog::error("Failed to process message: {}", e.what());

        error_message = e.what();
        has_error = true;
    }

    if (has_error) {
        co_await send_message("error",
                              {{"error", "message_processing_failed"},
                               {"message",
                                std::string("Failed to process message: ") + error_message}});
    }
}
} // namespace lansend::ipc
