#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <core/constant/route.h>
#include <core/security/certificate_manager.h>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = boost::asio::ip::tcp;

namespace lansend::core {

class HttpsClient {
public:
    HttpsClient(net::io_context& ioc, CertificateManager& cert_manager);
    ~HttpsClient();

    HttpsClient(const HttpsClient&) = delete;
    HttpsClient& operator=(const HttpsClient&) = delete;

    net::awaitable<bool> Connect(std::string_view host, unsigned short port);

    net::awaitable<bool> Disconnect();

    bool IsConnected() const;

    std::optional<boost::asio::ip::tcp::endpoint> local_endpoint() const;

    std::string current_host() const;

    unsigned short current_port() const;

    template<typename RequestBody>
    net::awaitable<http::response<http::string_body>> SendRequest(http::request<RequestBody>& req);

    template<typename Body>
    http::request<Body> CreateRequest(http::verb method,
                                      const std::string& target,
                                      bool keepAlive = true);

private:
    net::io_context& ioc_;
    CertificateManager& cert_manager_;
    ssl::context ssl_ctx_;
    std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> connection_;
    std::string current_host_;
    unsigned short current_port_ = 0;
    SSL_SESSION* ssl_session_ = nullptr;
};

template<typename RequestBody>
net::awaitable<http::response<http::string_body>> HttpsClient::SendRequest(
    http::request<RequestBody>& req) {
    if (!connection_) {
        throw std::runtime_error("No active connection");
    }

    co_await http::async_write(*connection_, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    co_await http::async_read(*connection_, buffer, res);

    co_return res;
}

template<typename Body>
http::request<Body> HttpsClient::CreateRequest(http::verb method,
                                               const std::string& target,
                                               bool keepAlive) {
    http::request<Body> req{method, target, 11};

    if (!current_host_.empty()) {
        req.set(http::field::host, current_host_ + ":" + std::to_string(current_port_));
    }

    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    if (target == ApiRoute::kSendChunk) {
        req.set(http::field::content_type, "application/octet-stream");
    } else {
        req.set(http::field::content_type, "application/json");
    }

    req.keep_alive(keepAlive);

    return req;
}

} // namespace lansend::core