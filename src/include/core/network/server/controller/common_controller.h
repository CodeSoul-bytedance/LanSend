#pragma once

#include "core/model/feedback.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <core/model.h>
#include <core/network/server/http_server.h>

namespace lansend::core {

class CommonController {
public:
    CommonController(HttpServer& server, FeedbackCallback callback = nullptr);
    ~CommonController() = default;

    void SetFeedbackCallback(FeedbackCallback callback);

private:
    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> onPing(
        const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> onConnect(
        const boost::beast::http::request<boost::beast::http::string_body>& req);

    void InstallRoutes(HttpServer& server);

    FeedbackCallback callback_;

    void feedback(Feedback&& feedback) {
        if (callback_) {
            callback_(std::move(feedback));
        }
    }
};

} // namespace lansend::core