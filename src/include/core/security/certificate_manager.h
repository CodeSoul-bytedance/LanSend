#pragma once

#include <boost/asio/ssl/context.hpp>
#include <core/model/security_context.h>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace lansend::core {

class CertificateManager {
public:
    CertificateManager(const std::filesystem::path& certDir);

    const SecurityContext& security_context() const;

    static std::string CalculateCertificateHash(const std::string& certificatePem);

    bool VerifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx);

    void TrustFingerprint(const std::string& fingerprint);

    bool IsFingerprintTrusted(const std::string& fingerprint);

    bool TrustCertificate(const std::string& certificatePem);

private:
    bool initSecurityContext();

    bool generateSelfSignedCertificate();

    bool saveSecurityContext();

    bool loadSecurityContext();

    void loadTrustedFingerprints();

    void saveTrustedFingerprints();

    SecurityContext security_context_;
    std::filesystem::path certificate_dir_;
    std::filesystem::path trusted_fingerprints_path_;

    std::unordered_set<std::string> trusted_fingerprints_;

    static constexpr int kCertValidityDays = 3650;
};

} // namespace lansend::core