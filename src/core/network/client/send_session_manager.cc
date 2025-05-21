#include <boost/asio.hpp>
#include <core/network/client/send_session_manager.h>
#include <core/security/certificate_manager.h>
#include <exception>

namespace net = boost::asio;

namespace lansend::core {

SendSessionManager::SendSessionManager(boost::asio::io_context& ioc,
                                       CertificateManager& cert_manager)
    : ioc_(ioc)
    , cert_manager_(cert_manager) {};

void SendSessionManager::SendFiles(std::string_view host,
                                   unsigned short port,
                                   const std::vector<std::filesystem::path>& file_paths) {
    auto send_session = std::make_shared<SendSession>(ioc_, cert_manager_);
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

} // namespace lansend::core