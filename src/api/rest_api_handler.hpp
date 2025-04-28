#pragma once

// #include "../core/network_manager.hpp"
#include "../transfer/transfer_manager.hpp"
#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <string>

namespace http = boost::beast::http;

class NetworkManager;
class TransferManager;

class RestApiHandler {
public:
    RestApiHandler(NetworkManager& network_manager, TransferManager& transfer_manager);
    ~RestApiHandler();

    // 设备信息
    boost::asio::awaitable<http::response<http::string_body>> handle_info_request(
        const http::request<http::string_body>& req);

    // 文件传输
    boost::asio::awaitable<http::response<http::string_body>> handle_send_request(
        const http::request<http::string_body>& req);
    boost::asio::awaitable<http::response<http::string_body>> handle_accept_request(
        const http::request<http::string_body>& req);
    boost::asio::awaitable<http::response<http::string_body>> handle_reject_request(
        const http::request<http::string_body>& req);
    boost::asio::awaitable<http::response<http::string_body>> handle_send_file(
        const http::request<http::string_body>& req);
    boost::asio::awaitable<http::response<http::string_body>> handle_finish_transfer(
        const http::request<http::string_body>& req);
    boost::asio::awaitable<http::response<http::string_body>> handle_cancel_transfer(
        const http::request<http::string_body>& req);

    // 为Electron预留的事件流接口
    boost::asio::awaitable<http::response<http::string_body>> handle_events_stream(
        const http::request<http::string_body>& req);

private:
    NetworkManager& network_manager_;
    TransferManager& transfer_manager_;
    Config& config_;
    Logger& logger_;

    // 辅助函数
    std::string get_device_id() const;
    std::string get_device_name() const;
    std::string get_certificate_fingerprint() const;
};