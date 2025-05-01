#pragma once

// #include "../core/network_manager.hpp"
#include "../transfer/transfer_manager.hpp"
#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <cstdint>
#include <functional>
#include <string>

namespace http = boost::beast::http;

class NetworkManager;
class TransferManager;

class RestApiHandler {
public:
    RestApiHandler(NetworkManager& network_manager, TransferManager& transfer_manager);
    ~RestApiHandler();

    // Device Information
    boost::asio::awaitable<http::response<http::string_body>> handle_info_request(
        const http::request<http::string_body>& req);

    // File Transfer Initiation & Control
    // 发起传输请求，预期响应中包含 transfer_id
    boost::asio::awaitable<http::response<http::string_body>> handle_send_request(
        const http::request<http::string_body>& req);
    boost::asio::awaitable<http::response<http::string_body>> handle_accept_request(
        const http::request<http::string_body>& req); // 可能需要传递 transfer_id
    boost::asio::awaitable<http::response<http::string_body>> handle_reject_request(
        const http::request<http::string_body>& req); // 可能需要传递 transfer_id
    boost::asio::awaitable<http::response<http::string_body>> handle_finish_transfer(
        const http::request<http::string_body>& req); // 需要传递 transfer_id
    boost::asio::awaitable<http::response<http::string_body>> handle_cancel_transfer(
        const http::request<http::string_body>& req); // 需要传递 transfer_id

    // 根据transfer_id查询传输状态
    boost::asio::awaitable<http::response<http::string_body>> handle_get_transfer_status(
        const http::request<http::string_body>& req, uint64_t transfer_id);

    // 根据chunk_index发送对应文件块
    // 注意：请求体可能需要是二进制类型，例如 http::vector_body<char> 或 http::buffer_body
    boost::asio::awaitable<http::response<http::empty_body>>
    handle_upload_chunk( // 返回 empty_body 可能更合适
        const http::request<http::vector_body<char>>& req,
        uint64_t transfer_id,
        uint64_t chunk_index);

    // 下载文件块 (向发送方拉取特定的文件块)
    // 响应体是块数据，例如 http::vector_body<char>
    boost::asio::awaitable<http::response<http::vector_body<char>>> handle_download_chunk(
        const http::request<http::string_body>& req, uint64_t transfer_id, uint64_t chunk_index);

    // Event Stream for Electron
    boost::asio::awaitable<http::response<http::string_body>> handle_events_stream(
        const http::request<http::string_body>& req);

private:
    NetworkManager& network_manager_;
    TransferManager& transfer_manager_;
    Config& config_;
    Logger& logger_;

    // Helper Functions
    std::string get_device_id() const;
    std::string get_device_name() const;
    std::string get_certificate_fingerprint() const;
};