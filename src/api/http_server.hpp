#pragma once

#include "../security/certificate_manager.hpp"
#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <map>
#include <string>

namespace http = boost::beast::http;

class HttpServer {
public:
    using RouteHandler = std::function<boost::asio::awaitable<http::response<http::string_body>>(
        const http::request<http::string_body>&)>;

    HttpServer(boost::asio::io_context& ioc, CertificateManager& cert_manager);
    ~HttpServer();

    // 路由管理
    void add_route(const std::string& path, http::verb method, RouteHandler handler);

    // 服务器控制
    void start(uint16_t port);
    void stop();

private:
    boost::asio::io_context& io_context_;
    Config& config_;
    Logger& logger_;
    CertificateManager& cert_manager_;

    boost::asio::ssl::context ssl_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::map<std::string, std::map<http::verb, RouteHandler>> routes_;

    // 协程任务
    boost::asio::awaitable<void> listener();
    boost::asio::awaitable<void> session(boost::asio::ip::tcp::socket socket);

    // 辅助函数
    std::string get_route_key(const std::string& path, http::verb method) const;
};