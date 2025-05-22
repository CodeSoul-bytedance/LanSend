#pragma once

#include <boost/asio/ssl/context.hpp>
#include <core/model/security_context.h>
#include <filesystem>
#include <string>

#define CERTIFICATE_MANAGER_ALLOW_UNREGISTERED 1
namespace lansend::core {

class CertificateManager {
public:
    CertificateManager(const std::filesystem::path& certDir);

    const SecurityContext& security_context() const;

    static std::string CalculateCertificateHash(const std::string& certificatePem);

    bool VerifyCertificate(bool preverified,
                           boost::asio::ssl::verify_context& ctx,
                           const std::string& ip,
                           uint16_t port);

    void RegisterDeviceFingerprint(const std::string& ip,
                                   uint16_t port,
                                   const std::string& fingerprint);
    void RemoveDeviceFingerprint(const std::string& ip, uint16_t port);
    std::optional<std::string> GetDeviceFingerprint(const std::string& ip, uint16_t port) const;

    bool IsUnregisteredAllowed() const { return unregistered_allowed_; }
    void SetUnregisteredAllowed(bool allow) { unregistered_allowed_ = allow; }

private:
    bool initSecurityContext();
    bool generateSelfSignedCertificate();
    bool saveSecurityContext();
    bool loadSecurityContext();

    std::string makeDeviceKey(const std::string& ip, uint16_t port) const;

    SecurityContext security_context_;
    std::filesystem::path certificate_dir_;
    bool unregistered_allowed_ = false;

    std::unordered_map<std::string, std::string> device_fingerprints_;

    static constexpr int kCertValidityDays = 3650;
};

} // namespace lansend::core