#include "core/model/device_info.h"
#include "core/model/feedback.h"
#include <boost/beast/http/string_body_fwd.hpp>
#include <core/constant/route.h>
#include <core/model.h>
#include <core/network/server/controller/common_controller.h>
#include <core/network/server/http_server.h>
#include <core/util/config.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace net = boost::asio;
namespace http = boost::beast::http;
using json = nlohmann::json;

namespace lansend::core {

CommonController::CommonController(HttpServer& server, FeedbackCallback callback)
    : feedback_callback_(callback) {
    InstallRoutes(server);
}

void CommonController::SetFeedbackCallback(FeedbackCallback callback) {
    feedback_callback_ = callback;
}

net::awaitable<http::response<http::string_body>> CommonController::onPing(
    const http::request<http::string_body>& req) {
    spdlog::debug("CommonController::OnPing");
    co_return HttpServer::Ok(req.version(), req.keep_alive());
}

net::awaitable<http::response<http::string_body>> CommonController::onConnect(
    const http::request<http::string_body>& req) {
    spdlog::debug("CommonController::OnConnect");
    json data;
    std::string pin_code;
    DeviceInfo device_info;
    try {
        data = json::parse(req.body());
        pin_code = data["pin_code"].get<std::string>();
        nlohmann::from_json(data["device_info"], device_info);
    } catch (const std::exception& e) {
        spdlog::error("Error parsing request: {}", e.what());
        co_return HttpServer::BadRequest(req.version(), req.keep_alive(), "invalid data");
    }
    // Check if the auth code matches
    if (settings.pin_code.empty()) {
        spdlog::info("Pin code is empty, reject connection from {} ({}:{})",
                     device_info.hostname,
                     device_info.ip_address,
                     device_info.port);
        http::response<http::string_body> res{http::status::forbidden, req.version()};
        co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "Auth code is empty");
    }
    if (settings.pin_code != pin_code) {
        spdlog::info("Pin code mismatch, reject connection from {} ({}:{})",
                     device_info.hostname,
                     device_info.ip_address,
                     device_info.port);
        http::response<http::string_body> res{http::status::forbidden, req.version()};
        co_return HttpServer::Forbidden(req.version(), req.keep_alive(), "Auth code mismatch");
    }
    spdlog::info("Connection accepted from {} ({}:{})",
                 device_info.hostname,
                 device_info.ip_address,
                 device_info.port);
    co_return HttpServer::Ok(req.version(), req.keep_alive(), "Connection accepted");
}

void CommonController::InstallRoutes(HttpServer& server) {
    server.AddRoute(ApiRoute::kPing.data(),
                    http::verb::get,
                    std::bind(&CommonController::onPing, this, std::placeholders::_1));
    server.AddRoute(ApiRoute::kConnect.data(),
                    http::verb::post,
                    std::bind(&CommonController::onConnect, this, std::placeholders::_1));
}

} // namespace lansend::core