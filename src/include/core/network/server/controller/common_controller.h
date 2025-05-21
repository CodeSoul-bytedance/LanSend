#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <core/network/server/http_server.h>

namespace lansend::core {

class CommonController {
public:
    CommonController(HttpServer& server);
    ~CommonController() = default;

private:
    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> onPing(
        const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> onConnect(
        const boost::beast::http::request<boost::beast::http::string_body>& req);

    void InstallRoutes(HttpServer& server);
};

} // namespace lansend::core