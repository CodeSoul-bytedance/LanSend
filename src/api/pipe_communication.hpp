#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#ifdef _WIN32
#include <boost/asio/windows/stream_handle.hpp>
#else
#include <boost/asio/posix/stream_descriptor.hpp>
#endif

namespace lansend {
namespace api {

class PipeCommunication {
public:
    // 消息处理器类型（接收JSON消息，返回JSON响应）
    using MessageHandler
        = std::function<boost::asio::awaitable<nlohmann::json>(const nlohmann::json&)>;

    // 构造函数
    explicit PipeCommunication(boost::asio::io_context& io_context,
                               const std::string& stdin_pipe_name,
                               const std::string& stdout_pipe_name);

    // 启动管道通信
    void start();

    // 注册消息处理器
    void register_handler(const std::string& message_type, MessageHandler handler);

    // 发送消息到前端
    boost::asio::awaitable<void> send_message(const std::string& type, const nlohmann::json& data);

private:
    // 处理从标准输入读取的消息
    boost::asio::awaitable<void> read_message_loop();

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
};

} // namespace api
} // namespace lansend
