#pragma once

#include <boost/asio/ssl/context.hpp>
#include <filesystem>
#include <models/SecurityContext.hpp>
#include <string>

namespace boost::asio::ssl {
class context;
}

class CertificateManager {
public:
    CertificateManager(const std::filesystem::path& cert_dir);
    ~CertificateManager();

    bool init_security_context();

    const SecurityContext& security_context() const;

    // Calculate the SHA-256 hash of the certificate
    static std::string calculate_certificate_hash(const std::string& certificate_pem);

    bool verify_remote_certificate(X509* cert, const std::string& expected_fingerprint);

    void set_current_hostname(const std::string& hostname);
    std::string current_hostname() const;

private:
    bool generate_self_signed_certificate();

    bool save_security_context();

    bool load_security_context();

    SecurityContext security_context_;
    std::filesystem::path certificate_dir_;

    // Hostname of the current connection
    std::string current_hostname_;
    mutable std::mutex hostname_mutex_;

    static constexpr int kCertValidityDays = 3650;
};