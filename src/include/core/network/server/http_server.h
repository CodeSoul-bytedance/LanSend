#pragma once

#include "core/security/certificate_manager.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <core/model/feedback.h>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace lansend::core {

class CommonController;
class ReceiveController;

using StringRequest = boost::beast::http::request<boost::beast::http::string_body>;
using BinaryRequest = boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>;

using HttpResponse = boost::beast::http::response<boost::beast::http::string_body>;

using StringRequestHandler = std::function<boost::asio::awaitable<HttpResponse>(StringRequest&&)>;
using BinaryRequestHandler = std::function<boost::asio::awaitable<HttpResponse>(BinaryRequest&&)>;

using HttpRequest = BinaryRequest;
using RouteHandler = BinaryRequestHandler;

enum class RequestType {
    kString,
    kBinary,
};

// 路由信息结构体
struct RouteInfo {
    boost::beast::http::verb method;
    RequestType type;
    std::variant<StringRequestHandler, BinaryRequestHandler> handler;
};

//HTTPS 服务器类
class HttpServer {
public:
    using ReceiveWaitConditionFunc = std::function<std::optional<std::vector<std::string>>()>;
    using ReceiveCancelConditionFunc = std::function<bool()>;

    // 构造函数
    HttpServer(boost::asio::io_context& io_context, CertificateManager& cert_manager);

    // 析构函数
    ~HttpServer();

    // 添加路由
    void AddRoute(const std::string& path,
                  boost::beast::http::verb method,
                  BinaryRequestHandler&& handler);
    void AddRoute(const std::string& path,
                  boost::beast::http::verb method,
                  StringRequestHandler&& handler);

    // 启动服务器
    void Start(uint16_t port);

    // 停止服务器
    void Stop();

    void SetFeedbackCallback(FeedbackCallback callback);
    void SetReceiveWaitConditionFunc(ReceiveWaitConditionFunc func);
    void SetReceiveCancelConditionFunc(ReceiveCancelConditionFunc func);

    static HttpResponse Ok(unsigned int version, bool keep_alive, std::string_view body = {});
    static HttpResponse NotFound(unsigned int version,
                                 bool keep_alive,
                                 std::string_view error_message = "Not Found");
    static HttpResponse BadRequest(unsigned int version,
                                   bool keep_alive,
                                   std::string_view error_message = "Bad Request");
    static HttpResponse InternalServerError(
        unsigned int version,
        bool keep_alive,
        std::string_view error_message = "Internal Server Error");
    static HttpResponse Forbidden(unsigned int version,
                                  bool keep_alive,
                                  std::string_view error_message = "Forbidden");
    static HttpResponse MethodNotAllowed(unsigned int version,
                                         bool keep_alive,
                                         std::string_view error_message = "Method Not Allowed");

    // 获取接收控制器
    ReceiveController& GetReceiveController() { return *receive_controller_; }

private:
    // 接受连接
    boost::asio::awaitable<void> acceptConnections();

    // 处理连接
    boost::asio::awaitable<void> handleConnection(
        boost::asio::ssl::stream<boost::beast::tcp_stream> stream);

    // 处理请求
    boost::asio::awaitable<HttpResponse> handleRequest(HttpRequest&& request);

    static StringRequest binaryToStringRequest(const BinaryRequest& req);

    boost::asio::io_context& io_context_;
    CertificateManager& cert_manager_;
    boost::asio::ssl::context ssl_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    bool running_;
    std::map<std::string, RouteInfo> routes_;
    std::unique_ptr<CommonController> common_controller_;
    std::unique_ptr<ReceiveController> receive_controller_;
};

} // namespace lansend::core
