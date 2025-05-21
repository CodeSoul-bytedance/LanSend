#include "core/constant/route.h"
#include "core/model/device_info.h"
#include "core/network/client/http_client.h"
#include <boost/asio.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <core/network/client/http_client_service.h>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
using json = nlohmann::json;

namespace lansend::core {

HttpClientService::HttpClientService(boost::asio::io_context& ioc,
                                     CertificateManager& cert_manager,
                                     FeedbackCallback callback)
    : ioc_(ioc)
    , cert_manager_(cert_manager)
    , send_session_manager_(ioc, cert_manager)
    , callback_(callback) {
    // Constructor implementation
}

void HttpClientService::Ping(std::string_view host, unsigned short port) {
    auto client_ptr = std::make_shared<HttpsClient>(ioc_, cert_manager_);
    net::co_spawn(
        ioc_,
        [&]() -> net::awaitable<void> {
            try {
                co_await client_ptr->Connect(host, port);
                if (client_ptr->IsConnected()) {
                    auto req = client_ptr->CreateRequest<http::string_body>(http::verb::get,
                                                                            ApiRoute::kPing.data(),
                                                                            true);
                    req.set(http::field::user_agent, "Lansend");
                    req.prepare_payload();

                    auto res = co_await client_ptr->SendRequest(req);
                    if (res.result() == http::status::ok) {
                        if (callback_) {
                            callback_(res.body());
                        }
                    } else {
                        if (callback_) {
                            callback_(std::string("Ping failed: ") + res.body());
                        }
                    }
                    co_await client_ptr->Disconnect();
                } else {
                    if (callback_) {
                        callback_(std::string("Connection failed"));
                    }
                }
            } catch (const std::exception& e) {
                if (callback_) {
                    callback_(std::string("Error: ") + e.what());
                }
            }
        },
        net::detached);
}

void HttpClientService::ConnectDevice(const std::string& pin_code, const DeviceInfo& device_info) {
    auto client_ptr = std::make_shared<HttpsClient>(ioc_, cert_manager_);
    net::co_spawn(
        ioc_,
        [&]() -> net::awaitable<void> {
            try {
                co_await client_ptr->Connect("localhost", device_info.port);
                if (client_ptr->IsConnected()) {
                    json data;
                    data["pin_code"] = pin_code;
                    data["device_info"] = device_info;

                    auto req = client_ptr->CreateRequest<http::string_body>(http::verb::post,
                                                                            ApiRoute::kConnect.data(),
                                                                            true);
                    req.body() = data.dump();
                    req.prepare_payload();

                    auto res = co_await client_ptr->SendRequest(req);
                    if (res.result() == http::status::ok) {
                        if (callback_) {
                            callback_(res.body());
                        }
                    } else {
                        if (callback_) {
                            callback_(std::string("Connection failed: ") + res.body());
                        }
                    }
                    co_await client_ptr->Disconnect();
                } else {
                    if (callback_) {
                        callback_(std::string("Connection failed"));
                    }
                }
            } catch (const std::exception& e) {
                if (callback_) {
                    callback_(std::string("Error: ") + e.what());
                }
            }
        },
        net::detached);
}

void HttpClientService::SendFiles(std::string_view ip_address,
                                  unsigned short port,
                                  const std::vector<std::filesystem::path>& file_paths) {
    send_session_manager_.SendFiles(ip_address, port, file_paths);
}

void HttpClientService::CancelSend(const std::string& session_id) {
    send_session_manager_.CancelSend(session_id);
}

} // namespace lansend::core