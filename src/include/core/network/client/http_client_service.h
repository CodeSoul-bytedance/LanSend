#pragma once

#include "send_session_manager.h"
#include <boost/asio/io_context.hpp>
#include <core/model.h>
#include <core/security/certificate_manager.h>
#include <string>
#include <string_view>

namespace lansend::core {

class HttpClientService {
public:
    HttpClientService(boost::asio::io_context& ioc,
                      CertificateManager& cert_manager,
                      FeedbackCallback callback = nullptr);
    ~HttpClientService() = default;

    void SetFeedbackCallback(FeedbackCallback callback);

    void Ping(std::string_view host, unsigned short port);

    void ConnectDevice(std::string_view pin_code,
                       std::string_view ip,
                       unsigned short port,
                       std::string_view device_id = {});

    void SendFiles(std::string_view ip_address,
                   unsigned short port,
                   const std::vector<std::filesystem::path>& file_paths,
                   std::string_view device_id = {});

    void CancelSend(const std::string& session_id);

    void CancelWaitForConfirmation(std::string_view ip, unsigned short port);

private:
    boost::asio::io_context& ioc_;
    CertificateManager& cert_manager_;
    SendSessionManager send_session_manager_;
    FeedbackCallback callback_;

    void feedback(Feedback&& feedback) {
        if (callback_) {
            callback_(std::move(feedback));
        }
    }
};

} // namespace lansend::core