#include "core/constant/route.h"
#include "core/model/device_info.h"
#include "core/model/feedback/device_connect_result.h"
#include "core/network/client/http_client.h"
#include <boost/asio.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <core/network/client/http_client_service.h>
#include <iostream>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
using json = nlohmann::json;

namespace lansend::core {

HttpClientService::HttpClientService(boost::asio::io_context& ioc,
                                     CertificateManager& cert_manager,
                                     FeedbackCallback callback)
    : ioc_(ioc)
    , cert_manager_(cert_manager)
    , send_session_manager_(ioc, cert_manager, callback)
    , callback_(callback) {
    // Constructor implementation
}

void HttpClientService::SetFeedbackCallback(FeedbackCallback callback) {
    callback_ = callback;
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
                        spdlog::info("Ping to {}:{} success", host, port);
                    } else {
                        spdlog::error("Ping to {}:{} failed: {}", host, port, res.body());
                    }
                    co_await client_ptr->Disconnect();
                } else {
                    spdlog::error("Ping to {}:{} failed: connection error", host, port);
                }
            } catch (const std::exception& e) {
                spdlog::error("Ping to {}:{} failed: {}", host, port, e.what());
            }
        },
        net::detached);
}

void HttpClientService::ConnectDevice(std::string_view pin_code,
                                      std::string_view ip,
                                      unsigned short port,
                                      std::string_view device_id) {
    auto client_ptr = std::make_shared<HttpsClient>(ioc_, cert_manager_);
    net::co_spawn(
        ioc_,
        [&]() -> net::awaitable<void> {
            try {
                co_await client_ptr->Connect(ip, port);
                if (client_ptr->IsConnected()) {
                    json data;
                    data["pin_code"] = pin_code;
                    data["device_info"] = DeviceInfo::LocalDeviceInfo();

                    auto req = client_ptr->CreateRequest<http::string_body>(http::verb::post,
                                                                            ApiRoute::kConnect.data(),
                                                                            true);
                    req.body() = data.dump();
                    req.prepare_payload();

                    auto res = co_await client_ptr->SendRequest(req);
                    if (res.result() == http::status::ok) {
                        spdlog::info("connect to device {}:{} success", ip, port);
                        feedback(Feedback{.type = FeedbackType::kConnectDeviceResult,
                                          .data = feedback::DeviceConnectResult{
                                              .device_id = device_id.data(),
                                          }});
                    } else {
                        spdlog::error("connect to device {}:{} failed, pin-code mismatch", ip, port);
                        feedback(Feedback{.type = FeedbackType::kConnectDeviceResult,
                                          .data = feedback::DeviceConnectResult{
                                              .device_id = device_id.data(),
                                              .success = false,
                                              .pin_code_error = true,
                                          }});
                    }
                    co_await client_ptr->Disconnect();
                } else {
                    spdlog::error("connect to device {}:{} failed, network error", ip, port);
                    feedback(Feedback{.type = FeedbackType::kConnectDeviceResult,
                                      .data = feedback::DeviceConnectResult{
                                          .device_id = device_id.data(),
                                          .success = false,
                                          .network_error = true,
                                      }});
                }
            } catch (const std::exception& e) {
                spdlog::error("connect to device {}:{} failed: {}", ip, port, e.what());
                feedback(Feedback{.type = FeedbackType::kConnectDeviceResult,
                                  .data = feedback::DeviceConnectResult{
                                      .device_id = device_id.data(),
                                      .success = false,
                                      .network_error = true,
                                  }});
            }
        },
        net::detached);
}

void HttpClientService::SendFiles(std::string_view ip_address,
                                  unsigned short port,
                                  const std::vector<std::filesystem::path>& file_paths,
                                  std::string_view device_id) {
    send_session_manager_.SendFiles(ip_address, port, file_paths, device_id);
}

void HttpClientService::CancelSend(const std::string& session_id) {
    send_session_manager_.CancelSend(session_id);
}

void HttpClientService::CancelWaitForConfirmation(std::string_view ip, unsigned short port) {
    send_session_manager_.CancelWaitForConfirmation(ip, port);
}

} // namespace lansend::core