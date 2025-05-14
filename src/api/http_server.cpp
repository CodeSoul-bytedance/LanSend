
#include "http_server.hpp"
#include "../utils/logger.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

namespace lansend {
namespace api {

// Introduce awaitable operators for convenience.
using namespace boost::asio::experimental::awaitable_operators;

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

HttpServer::HttpServer(net::io_context& io_context, ssl::context& ssl_context)
    : io_context_(io_context)
    , ssl_context_(ssl_context)
    , acceptor_(io_context)
    , running_(false) {
    spdlog::info("HttpServer created.");
}

HttpServer::~HttpServer() {
    if (running_) {
        stop();
    }
    spdlog::info("HttpServer destroyed.");
}

void HttpServer::add_route(const std::string& path, http::verb method, RouteHandler handler) {
    routes_[path] = {method, std::move(handler)};
    spdlog::info(std::format("Added route: {} {}", std::string(http::to_string(method)), path));
}

void HttpServer::start(uint16_t port) {
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

        net::co_spawn(io_context_, accept_connections(), net::detached);

    } catch (const std::exception& e) {
        spdlog::error(std::format("Failed to start server on port {}: {}", port, e.what()));
        running_ = false;
        if (acceptor_.is_open()) {
            acceptor_.close();
        }
    }
}

void HttpServer::stop() {
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

boost::asio::awaitable<void> HttpServer::accept_connections() {
    while (running_) {
        try {
            tcp::socket socket = co_await acceptor_.async_accept(net::use_awaitable);
            spdlog::info(std::format("Accepted connection from: {}",
                                     std::string(socket.remote_endpoint().address().to_string())));

            ssl::stream<tcp::socket> stream(std::move(socket), ssl_context_);

            net::co_spawn(io_context_, handle_connection(std::move(stream)), net::detached);
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

boost::asio::awaitable<void> HttpServer::handle_connection(ssl::stream<tcp::socket> stream) {
    auto executor = co_await net::this_coro::executor;
    net::steady_timer timer(executor);
    beast::error_code timer_ec;

    try {
        co_await stream.async_handshake(ssl::stream_base::server, net::use_awaitable);
        spdlog::info("SSL handshake successful.");

        beast::flat_buffer buffer;
        constexpr std::chrono::seconds keep_alive_timeout{120};

        while (stream.next_layer().is_open()) {
            timer.expires_after(keep_alive_timeout);
            timer_ec.clear();

            HttpRequest req;
            std::variant<std::monostate, std::size_t> result = co_await (
                timer.async_wait(net::redirect_error(net::use_awaitable, timer_ec))
                || http::async_read(stream, buffer, req, net::use_awaitable));

            if (result.index() == 0) {
                if (!timer_ec) {
                    spdlog::info(std::format("Keep-alive timeout after {}s. Closing connection.",
                                             keep_alive_timeout.count()));
                    break;
                } else if (timer_ec == net::error::operation_aborted) {
                    spdlog::info(
                        "Keep-alive timer cancelled (operation_aborted). Connection closing.");
                    break;
                } else {
                    spdlog::error(std::format("Keep-alive timer error: {}. Closing connection.",
                                              timer_ec.message()));
                    break;
                }
            }

            timer.cancel();

            spdlog::info(std::format("Received request: {} {}",
                                     std::string(http::to_string(req.method())),
                                     std::string(req.target())));

            AnyResponse res = co_await handle_request(std::move(req));

            bool keep_alive = std::visit([](const auto& response) { return response.keep_alive(); },
                                         res);

            co_await std::visit(
                [&](auto&& response) {
                    return http::async_write(stream,
                                             std::forward<decltype(response)>(response),
                                             net::use_awaitable);
                },
                std::move(res));

            if (!keep_alive) {
                break;
            }
        }
    } catch (const boost::system::system_error& e) {
        if (e.code() == net::error::eof || e.code() == ssl::error::stream_truncated
            || e.code() == http::error::end_of_stream || e.code() == net::error::connection_reset) {
            spdlog::info(std::format("Connection closed by peer: {}", e.what()));
        } else if (e.code() != net::error::operation_aborted) {
            spdlog::error(std::format("Connection error: {}", e.what()));
        }
    } catch (const std::exception& e) {
        spdlog::error(std::format("Unexpected error handling connection: {}", e.what()));
    }

    try {
        timer.cancel();

        co_await stream.async_shutdown(net::use_awaitable);
    } catch (const boost::system::system_error& e) {
        if (e.code() != net::error::eof && e.code() != ssl::error::stream_truncated
            && e.code() != net::error::connection_reset
            && e.code() != net::error::operation_aborted) {
            spdlog::warn(std::format("Error during SSL shutdown: {}", e.what()));
        }
    } catch (const std::exception& e) {
        spdlog::warn(std::format("Unexpected error during SSL shutdown: {}", e.what()));
    }
    spdlog::info("Connection handling finished.");
}

boost::asio::awaitable<AnyResponse> HttpServer::handle_request(HttpRequest&& req) {
    AnyResponse res;
    std::visit(
        [&](auto&& response) {
            response.version(req.version());
            response.keep_alive(req.keep_alive());
            response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            response.set(http::field::content_type, "application/json");
        },
        res);

    std::string path(req.target());
    auto it = routes_.find(path);

    if (it == routes_.end()) {
        spdlog::warn(std::format("Route not found: {}", path));
        http::response<http::string_body> not_found_response;
        not_found_response.version(req.version());
        not_found_response.keep_alive(req.keep_alive());
        not_found_response.result(http::status::not_found);
        not_found_response.set(http::field::content_type, "application/json");
        not_found_response.body() = "{\"error\": \"Not Found\"}";
        not_found_response.prepare_payload();

        res = std::move(not_found_response);

        co_return res;
    }

    const auto& route_info = it->second;
    if (route_info.method != req.method()) {
        spdlog::warn(std::format("Method not allowed for route {}: requested {}, expected {}",
                                 path,
                                 std::string(http::to_string(req.method())),
                                 std::string(http::to_string(route_info.method))));
        http::response<http::string_body> error_res{http::status::method_not_allowed, req.version()};
        error_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        error_res.set(http::field::content_type, "application/json");
        error_res.set(http::field::allow, std::string(http::to_string(route_info.method)));
        error_res.keep_alive(req.keep_alive());
        error_res.body() = R"({\"error\": \"Method Not Allowed\"})";
        error_res.prepare_payload();
        res = std::move(error_res);
        co_return res;
    }

    auto request_version = req.version();
    bool request_keep_alive = req.keep_alive();

    try {
        res = co_await route_info.handler(std::move(req));
    } catch (const std::exception& e) {
        spdlog::error(std::format("Error executing handler for {}: {}", path, e.what()));

        http::response<http::string_body> error_res{http::status::internal_server_error,
                                                    request_version};
        error_res.keep_alive(request_keep_alive);
        error_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        error_res.set(http::field::content_type, "application/json");
        error_res.body() = R"({\"error\": \"Internal Server Error\"})";
        error_res.prepare_payload();

        res = std::move(error_res);
    }

    co_return res;
}

} // namespace api
} // namespace lansend

