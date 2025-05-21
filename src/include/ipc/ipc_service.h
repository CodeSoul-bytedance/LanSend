#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <iostream>
#include <ipc/ipc_event_stream.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

#ifdef _WIN32
#include <boost/asio/windows/stream_handle.hpp>
#else
#include <boost/asio/posix/stream_descriptor.hpp>
#endif

namespace lansend::ipc {

// receive events from the frontend application, post them to IpcEventStream
// poll notifications from IpcEventStream, send them to the frontend application
class IpcService {
public:
    // 消息处理器类型（接收JSON消息，返回JSON响应）
    using MessageHandler
        = std::function<boost::asio::awaitable<nlohmann::json>(const nlohmann::json&)>;

    // 构造函数
    explicit IpcService(boost::asio::io_context& io_context,
                        IpcEventStream& event_stream,
                        const std::string& stdin_pipe_name,
                        const std::string& stdout_pipe_name);

    // 启动管道通信
    void start();

    // 注册消息处理器
    void register_handler(const std::string& message_type, MessageHandler handler);

    // 发送消息到前端
    boost::asio::awaitable<void> send_message(const std::string& type, const nlohmann::json& data);

private:
    IpcEventStream& event_stream_;

    // 处理从标准输入读取的消息
    boost::asio::awaitable<void> read_message_loop();

    // 处理从event_stream读取的消息
    boost::asio::awaitable<void> read_event_stream_loop();

    // 处理接收到的消息
    boost::asio::awaitable<void> handle_message(const std::string& message_str);

private:
    boost::asio::io_context& io_context_;
#ifdef _WIN32
    boost::asio::windows::stream_handle input_;
    boost::asio::windows::stream_handle output_;
#else
    boost::asio::posix::stream_descriptor input_;
    boost::asio::posix::stream_descriptor output_;
#endif
    std::map<std::string, MessageHandler> handlers_;
    bool running_;
    std::mutex write_mutex_; // 用于同步对管道的写入操作
};

} // namespace lansend::ipc