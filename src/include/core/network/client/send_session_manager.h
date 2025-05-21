#pragma once

#include "send_session.h"
#include <boost/asio/io_context.hpp>
#include <core/security/certificate_manager.h>
#include <string>
#include <unordered_map>

namespace lansend::core {

class SendSessionManager {
public:
    SendSessionManager(boost::asio::io_context& ioc, CertificateManager& cert_manager);
    ~SendSessionManager() = default;
    SendSessionManager(const SendSessionManager&) = delete;
    SendSessionManager& operator=(const SendSessionManager&) = delete;

    void SendFiles(std::string_view,
                   unsigned short port,
                   const std::vector<std::filesystem::path>& file_paths);

    void CancelSend(const std::string& session_id);

    void CancelWaitForConfirmation(std::string_view ip, unsigned short port);

private:
    void addSendSession(std::shared_ptr<SendSession> session) {
        send_sessions_[session->session_id()] = std::move(session);
    }

    boost::asio::io_context& ioc_;
    CertificateManager& cert_manager_;

    std::unordered_map<std::string, std::shared_ptr<SendSession>> send_sessions_;
};

} // namespace lansend::core