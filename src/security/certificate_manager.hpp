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
    bool generate_self_signed_certificate(const std::filesystem::path& cert_path,
                                          const std::filesystem::path& key_path);
    bool load_certificate(const std::filesystem::path& cert_path,
                          const std::filesystem::path& key_path);
    std::string get_certificate_fingerprint() const;

    // SSL上下文
    boost::asio::ssl::context& get_ssl_context();

private:
    Config& config_;
    Logger& logger_;

    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
    std::string certificate_fingerprint_;

    // OpenSSL相关
    struct OpenSSLContext;
    std::unique_ptr<OpenSSLContext> openssl_context_;
};