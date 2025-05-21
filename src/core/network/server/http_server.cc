
#include "core/security/open_ssl_provider.h"
#include "spdlog/spdlog.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <core/constant/transfer.h>
#include <core/network/server/controller/common_controller.h>
#include <core/network/server/controller/receive_controller.h>
#include <core/network/server/http_server.h>

namespace lansend::core {

// Introduce awaitable operators for convenience.
using namespace boost::asio::experimental::awaitable_operators;

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

HttpServer::HttpServer(boost::asio::io_context& io_context, CertificateManager& cert_manager)
    : io_context_(io_context)
    , cert_manager_(cert_manager)
    , ssl_context_(
          OpenSSLProvider::BuildServerContext(cert_manager_.security_context().certificate_pem,
                                              cert_manager_.security_context().private_key_pem))
    , acceptor_(io_context)
    , running_(false) {
    common_controller_ = std::make_unique<CommonController>(*this);
    receive_controller_ = std::make_unique<ReceiveController>(*this);
    spdlog::info("HttpServer created.");
}

HttpServer::~HttpServer() {
    if (running_) {
        Stop();
    }
    spdlog::info("HttpServer destroyed.");
}

void HttpServer::AddRoute(const std::string& path,
                          http::verb method,
                          BinaryRequestHandler&& handler) {
    routes_[path] = {method, RequestType::kBinary, std::move(handler)};
    spdlog::info(std::format("Added route: {} {}", std::string(http::to_string(method)), path));
}

void HttpServer::AddRoute(const std::string& path,
                          boost::beast::http::verb method,
                          StringRequestHandler&& handler) {
    routes_[path] = {method, RequestType::kString, std::move(handler)};
    spdlog::info(std::format("Added route: {} {}", std::string(http::to_string(method)), path));
}

void HttpServer::Start(uint16_t port) {
    if (running_) {
        spdlog::warn("Server is already running.");
        return;
    }

    try {
        tcp::endpoint endpoint(tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
        running_ = true;
        spdlog::info(std::format("HTTPS Server started on port {}", port));

        net::co_spawn(io_context_, acceptConnections(), net::detached);

    } catch (const std::exception& e) {
        spdlog::error(std::format("Failed to start server on port {}: {}", port, e.what()));
        running_ = false;
        if (acceptor_.is_open()) {
            acceptor_.close();
        }
    }
}

void HttpServer::SetFeedbackCallback(FeedbackCallback callback) {
    common_controller_->SetFeedbackCallback(callback);
    receive_controller_->SetFeedbackCallback(callback);
}

void HttpServer::SetReceiveWaitConditionFunc(ReceiveWaitConditionFunc func) {
    receive_controller_->SetWaitConditionFunc(func);
}

void HttpServer::SetReceiveCancelConditionFunc(ReceiveCancelConditionFunc func) {
    receive_controller_->SetCancelConditionFunc(func);
}

void HttpServer::Stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    acceptor_.cancel();
    if (acceptor_.is_open()) {
        acceptor_.close();
    }
    spdlog::info("HTTPS Server stopped.");
}

HttpResponse HttpServer::Ok(unsigned int version, bool keep_alive, std::string_view body) {
    HttpResponse res{http::status::ok, version};
    res.keep_alive(keep_alive);
    res.set(http::field::content_type, "application/json");
    res.body() = body;
    res.prepare_payload();
    return res;
}

HttpResponse HttpServer::NotFound(unsigned int version,
                                  bool keep_alive,
                                  std::string_view error_message) {
    HttpResponse res{http::status::not_found, version};
    res.keep_alive(keep_alive);
    res.set(http::field::content_type, "text/plain");
    res.body() = error_message;
    res.prepare_payload();
    return res;
}

HttpResponse HttpServer::BadRequest(unsigned int version,
                                    bool keep_alive,
                                    std::string_view error_message) {
    HttpResponse res{http::status::bad_request, version};
    res.keep_alive(keep_alive);
    res.set(http::field::content_type, "text/plain");
    res.body() = error_message;
    res.prepare_payload();
    return res;
}

HttpResponse HttpServer::InternalServerError(unsigned int version,
                                             bool keep_alive,
                                             std::string_view error_message) {
    HttpResponse res{http::status::internal_server_error, version};
    res.keep_alive(keep_alive);
    res.set(http::field::content_type, "text/plain");
    res.body() = error_message;
    res.prepare_payload();
    return res;
}

HttpResponse HttpServer::Forbidden(unsigned int version,
                                   bool keep_alive,
                                   std::string_view error_message) {
    HttpResponse res{http::status::forbidden, version};
    res.keep_alive(keep_alive);
    res.set(http::field::content_type, "text/plain");
    res.body() = error_message;
    res.prepare_payload();
    return res;
}

HttpResponse HttpServer::MethodNotAllowed(unsigned int version,
                                          bool keep_alive,
                                          std::string_view error_message) {
    HttpResponse res{http::status::method_not_allowed, version};
    res.keep_alive(keep_alive);
    res.set(http::field::content_type, "text/plain");
    res.body() = error_message;
    res.prepare_payload();
    return res;
}

boost::asio::awaitable<void> HttpServer::acceptConnections() {
    while (running_) {
        try {
            tcp::socket socket = co_await acceptor_.async_accept();
            spdlog::info(std::format("Accepted connection from: {}",
                                     std::string(socket.remote_endpoint().address().to_string())));

            beast::ssl_stream<beast::tcp_stream> stream(beast::tcp_stream(std::move(socket)),
                                                        ssl_context_);

            net::co_spawn(io_context_, handleConnection(std::move(stream)), net::detached);
        } catch (const boost::system::system_error& e) {
            if (e.code() == net::error::operation_aborted) {
                spdlog::info("Accept operation cancelled.");
                break;
            } else {
                spdlog::error(std::format("Error accepting connection: {}", e.what()));
            }
        } catch (const std::exception& e) {
            spdlog::error(std::format("Unexpected error during accept: {}", e.what()));
        }
    }
    spdlog::info("Stopped accepting connections.");
}

boost::asio::awaitable<void> HttpServer::handleConnection(ssl::stream<beast::tcp_stream> stream) {
    try {
        auto& socket = stream.next_layer().socket();
        auto endpoint = socket.remote_endpoint();
        auto endpoint_ip_str = endpoint.address().to_string();
        spdlog::info("New connection from: {}:{}", endpoint_ip_str, endpoint.port());

        co_await stream.async_handshake(ssl::stream_base::server, net::use_awaitable);
        spdlog::debug("SSL handshake completed successfully");

        beast::flat_buffer buffer;
        bool keep_alive = true;

        while (keep_alive) {
            try {
                beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

                http::request_parser<http::vector_body<uint8_t>> parser;
                parser.body_limit(transfer::kMaxChunkSize + 8);

                spdlog::debug("Waiting for client request...");
                co_await http::async_read(stream, buffer, parser);
                auto req = parser.release();

                spdlog::info("Received {} request for {}", req.method_string(), req.target());

                keep_alive = req.keep_alive();

                HttpResponse res = co_await handleRequest(std::move(req));

                co_await http::async_write(stream, res);

                if (!keep_alive) {
                    spdlog::debug("Connection: close requested, ending session");
                    break;
                }
            } catch (const boost::system::system_error& e) {
                // Notify the receiver if the sender is lost
                if (receive_controller_->session_status() == ReceiveSessionStatus::kWorking) {
                    if (receive_controller_->sender_ip() == endpoint_ip_str
                        && receive_controller_->sender_port() == endpoint.port()) {
                        spdlog::error("Lost connection to sender {}:{} while receiving file",
                                      endpoint_ip_str,
                                      endpoint.port());
                        receive_controller_->NotifySenderLost();
                    }
                }

                if (e.code() == boost::beast::error::timeout || e.code() == boost::asio::error::eof
                    || e.code() == boost::asio::error::operation_aborted
                    || e.code() == boost::beast::http::error::end_of_stream) {
                    spdlog::debug("Connection closed by peer: {}", e.code().message());
                    break;
                }
                throw e;
            }
        }

        spdlog::debug("Closing connection gracefully");
        beast::error_code ec;
        stream.shutdown(ec);
        if (ec && ec != net::error::eof) {
            spdlog::debug("Shutdown notice: {}", ec.message());
        }
    } catch (const std::exception& e) {
        std::string error_msg = e.what();
        if (error_msg.find("end of stream") != std::string::npos
            || error_msg.find("stream truncated") != std::string::npos
            || error_msg.find("operation canceled") != std::string::npos) {
            spdlog::debug("Session ended: {}", error_msg);
        } else {
            spdlog::error("Session error: {}", error_msg);
        }
    }
    spdlog::info("Connection handling finished.");
}

boost::asio::awaitable<HttpResponse> HttpServer::handleRequest(HttpRequest&& req) {
    std::string path(req.target());
    auto it = routes_.find(path);

    if (it == routes_.end()) {
        spdlog::warn(std::format("Route not found: {}", path));
        co_return NotFound(req.version(), req.keep_alive());
    }

    const auto& route_info = it->second;
    if (route_info.method != req.method()) {
        spdlog::warn(std::format("Method not allowed for route {}: requested {}, expected {}",
                                 path,
                                 std::string(http::to_string(req.method())),
                                 std::string(http::to_string(route_info.method))));
        co_return MethodNotAllowed(req.version(), req.keep_alive());
    }

    auto request_version = req.version();
    bool request_keep_alive = req.keep_alive();

    try {
        HttpResponse res;
        if (route_info.type == RequestType::kString) {
            auto handler = std::get<StringRequestHandler>(route_info.handler);
            res = co_await handler(binaryToStringRequest(req));
        } else {
            auto handler = std::get<BinaryRequestHandler>(route_info.handler);
            res = co_await handler(std::move(req));
        }
        co_return res;
    } catch (const std::exception& e) {
        spdlog::error(std::format("Error executing handler for {}: {}", path, e.what()));
        co_return InternalServerError(request_version, request_keep_alive, e.what());
    }
}

StringRequest HttpServer::binaryToStringRequest(const BinaryRequest& req) {
    http::request<http::string_body> string_req;
    string_req.method(req.method());
    string_req.target(req.target());
    string_req.version(req.version());

    for (auto const& field : req)
        string_req.set(field.name(), field.value());

    std::string body_str(reinterpret_cast<const char*>(req.body().data()), req.body().size());
    string_req.body() = std::move(body_str);
    string_req.prepare_payload();

    return string_req;
}

} // namespace lansend::core