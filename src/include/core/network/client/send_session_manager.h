#pragma once

#include "core/model/feedback.h"
#include "send_session.h"
#include <boost/asio/io_context.hpp>
#include <core/security/certificate_manager.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace lansend::core {

class SendSessionManager {
public:
    SendSessionManager(boost::asio::io_context& ioc,
                       CertificateManager& cert_manager,
                       FeedbackCallback callback = nullptr);
    ~SendSessionManager() = default;
    SendSessionManager(const SendSessionManager&) = delete;
    SendSessionManager& operator=(const SendSessionManager&) = delete;

    void SendFiles(std::string_view,
                   unsigned short port,
                   const std::vector<std::filesystem::path>& file_paths,
                   std::string_view device_id = {});

    void CancelSend(const std::string& session_id);

    void CancelWaitForConfirmation(std::string_view ip, unsigned short port);

private:
    void addSendSession(std::shared_ptr<SendSession> session) {
        send_sessions_[session->session_id()] = std::move(session);
    }

    boost::asio::io_context& ioc_;
    CertificateManager& cert_manager_;
    FeedbackCallback callback_;

    std::unordered_map<std::string, std::shared_ptr<SendSession>> send_sessions_;

    void feedback(Feedback&& feedback) {
        if (callback_) {
            callback_(std::move(feedback));
        }
    }
};

} // namespace lansend::core