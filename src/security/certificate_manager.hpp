#pragma once

#include <boost/asio/ssl/context.hpp>
#include <filesystem>
#include <models/security_context.hpp>
#include <mutex>
#include <openssl/x509.h>
#include <string>
#include <unordered_map>

class CertificateManager {
public:
    CertificateManager(const std::filesystem::path& certDir);
    ~CertificateManager();

    bool init_security_context();

    const SecurityContext& security_context() const;

    static std::string calculate_certificate_hash(const std::string& certificatePem);

    bool verify_certificate(bool preverified, boost::asio::ssl::verify_context& ctx);

    void trust_host(const std::string& hostname, const std::string& fingerprint);

    bool is_host_trusted(const std::string& hostname, const std::string& fingerprint);

    void set_current_hostname(const std::string& hostname);
    std::string current_hostname() const;

private:
    bool generate_self_signed_certificate();

    bool save_security_context();

    bool load_security_context();

    void load_trusted_fingerprints();

    void save_trusted_fingerprints();

    SecurityContext security_context_;
    std::filesystem::path certificate_dir_;
    std::filesystem::path trusted_fingerprints_path_;

    std::unordered_map<std::string, std::string> trusted_hosts_;

    std::string current_hostname_;
    mutable std::mutex hostname_mutex_;

    static constexpr int kCertValidityDays = 3650;
};