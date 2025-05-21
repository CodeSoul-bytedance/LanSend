#pragma once

#include "core/model/device_info.h"
#include "core/network/client/send_session.h"
#include "send_session_manager.h"
#include <boost/asio/io_context.hpp>
#include <core/security/certificate_manager.h>

namespace lansend::core {

using FeedbackCallback = std::function<void(const nlohmann::json&)>;

class HttpClientService {
public:
    HttpClientService(boost::asio::io_context& ioc,
                      CertificateManager& cert_manager,
                      FeedbackCallback callback = nullptr);
    ~HttpClientService() = default;

    void Ping(std::string_view host, unsigned short port);

    void ConnectDevice(const std::string& pin_code, const DeviceInfo& device_info);

    void SendFiles(std::string_view ip_address,
                   unsigned short port,
                   const std::vector<std::filesystem::path>& file_paths);

    void CancelSend(const std::string& session_id);

private:
    boost::asio::io_context& ioc_;
    CertificateManager& cert_manager_;
    SendSessionManager send_session_manager_;
    FeedbackCallback callback_;
};

} // namespace lansend::core