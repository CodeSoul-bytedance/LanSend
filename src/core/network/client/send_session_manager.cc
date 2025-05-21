#include "core/model/feedback.h"
#include <boost/asio.hpp>
#include <core/network/client/send_session_manager.h>
#include <core/security/certificate_manager.h>
#include <exception>

namespace net = boost::asio;

namespace lansend::core {

SendSessionManager::SendSessionManager(boost::asio::io_context& ioc,
                                       CertificateManager& cert_manager,
                                       FeedbackCallback callback)
    : ioc_(ioc)
    , cert_manager_(cert_manager)
    , callback_(callback) {};

void SendSessionManager::SendFiles(std::string_view host,
                                   unsigned short port,
                                   const std::vector<std::filesystem::path>& file_paths,
                                   std::string_view device_id) {
    auto send_session = std::make_shared<SendSession>(ioc_, cert_manager_);
    send_session->RecordReceiverId(device_id);
    net::co_spawn(ioc_,
                  send_session->Start(file_paths,
                                      host,
                                      port,
                                      [this, send_session]() {
                                          this->addSendSession(send_session);
                                      }),
                  [this, send_session](std::exception_ptr p) {
                      // Clean up the session
                      const auto& session_id = send_session->session_id();
                      if (this->send_sessions_.contains(session_id)) {
                          if (this->send_sessions_[session_id]->session_status()
                              == SessionStatus::kSending) {
                              spdlog::warn("SendSession {} not sending, clean up anyway",
                                           session_id);
                          } else {
                              spdlog::debug("SendSession {} cleaned up", session_id);
                          }
                          this->send_sessions_.erase(session_id);
                      }
                  });
}

void SendSessionManager::CancelSend(const std::string& session_id) {
    if (send_sessions_.contains(session_id)) {
        send_sessions_[session_id]->Cancel();
    }
}

void SendSessionManager::CancelWaitForConfirmation(std::string_view ip, unsigned short port) {
    auto client_ptr = std::make_shared<HttpsClient>(ioc_, cert_manager_);
    net::co_spawn(
        ioc_,
        [&]() -> net::awaitable<void> {
            try {
                co_await client_ptr->Connect(ip, port);
                if (client_ptr->IsConnected()) {
                    auto req = client_ptr
                                   ->CreateRequest<http::string_body>(http::verb::get,
                                                                      ApiRoute::kCancelWait.data(),
                                                                      true);

                    nlohmann::json data;
                    auto& local_device = DeviceInfo::LocalDeviceInfo();
                    data["ip"] = local_device.ip_address;
                    data["port"] = local_device.port;
                    req.set(http::field::user_agent, "Lansend");
                    req.prepare_payload();

                    auto res = co_await client_ptr->SendRequest(req);
                    if (res.result() == http::status::ok) {
                        spdlog::info("cancel wait success");
                    } else {
                        spdlog::error("cancel wait failed");
                    }
                    co_await client_ptr->Disconnect();
                } else {
                    spdlog::error("cancel wait failed");
                }
            } catch (const std::exception& e) {
                spdlog::error("cancel wait failed: {}", e.what());
                feedback(Feedback{.type = FeedbackType::kNetworkError});
            }
        },
        net::detached);
}

} // namespace lansend::core