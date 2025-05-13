#pragma once

#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <boost/asio/ssl/context.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace boost::asio::ssl {
class context;
}

class CertificateManager {
public:
    CertificateManager();
    ~CertificateManager();

    // 证书管理
    bool generate_self_signed_certificate(const std::string& common_name,
                                          const std::filesystem::path& cert_path,
                                          const std::filesystem::path& key_path,
                                          int key_length = 2048,
                                          int expiry_days = 3650);
    bool load_certificate(const std::filesystem::path& cert_path,
                          const std::filesystem::path& key_path);
    std::string get_certificate_fingerprint() const;

    // SSL上下文
    boost::asio::ssl::context& get_ssl_context();

private:
    bool calculate_fingerprint();

    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
    std::string certificate_fingerprint_;
};