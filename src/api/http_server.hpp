#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/http.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace lansend {
namespace api {

namespace http = boost::beast::http;
using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;

// 路由处理器函数类型
using RouteHandler = std::function<boost::asio::awaitable<HttpResponse>(HttpRequest&&)>;

// 路由信息结构体
struct RouteInfo {
    http::verb method;
    RouteHandler handler;
};

/**
 * @brief HTTPS 服务器类
 * 
 * 管理 HTTPS 服务器，处理路由，管理 SSL 连接
 */
class HttpServer {
public:
    // 构造函数
    HttpServer(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context);

    // 析构函数
    ~HttpServer();

    // 添加路由
    void add_route(const std::string& path, http::verb method, RouteHandler handler);

    // 启动服务器
    void start(uint16_t port);

    // 停止服务器
    void stop();

private:
    // 接受连接
    boost::asio::awaitable<void> accept_connections();

    // 处理连接
    boost::asio::awaitable<void> handle_connection(
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream);

    // 处理请求
    boost::asio::awaitable<HttpResponse> handle_request(HttpRequest&& request);

private:
    boost::asio::io_context& io_context_;
    boost::asio::ssl::context& ssl_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    bool running_;
    std::map<std::string, RouteInfo> routes_;
};

} // namespace api
} // namespace lansend
