#include <core/security/certificate_manager.h>
#include <core/security/open_ssl_provider.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lansend::core {

CertificateManager::CertificateManager(const fs::path& certDir)
    : certificate_dir_(certDir) {
    if (!fs::exists(certDir)) {
        fs::create_directories(certDir);
    }

    OpenSSLProvider::InitOpenSSL();
    initSecurityContext();
}

bool CertificateManager::initSecurityContext() {
    if (loadSecurityContext()) {
        spdlog::info("Loaded existing certificate with fingerprint: {}",
                     security_context_.certificate_hash);
        return true;
    }

    if (generateSelfSignedCertificate()) {
        spdlog::info("Generated new self-signed certificate with fingerprint: {}",
                     security_context_.certificate_hash);
        return saveSecurityContext();
    }

    return false;
}

const SecurityContext& CertificateManager::security_context() const {
    return security_context_;
}

// Calculate certificate fingerprint using SHA-256
std::string CertificateManager::CalculateCertificateHash(const std::string& certificatePem) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;

    // Create EVP_MD_CTX context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, certificatePem.c_str(), certificatePem.size());
    EVP_DigestFinal_ex(mdctx, hash, &hashLen);
    EVP_MD_CTX_free(mdctx);

    // Convert to hexadecimal string
    std::stringstream ss;
    for (unsigned int i = 0; i < hashLen; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int) hash[i];
    }

    return ss.str();
}

bool CertificateManager::VerifyCertificate(bool preverified,
                                           boost::asio::ssl::verify_context& ctx,
                                           const std::string& ip,
                                           uint16_t port) {
    // Get the certificate being verified
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    if (!cert) {
        spdlog::error("No certificate to verify");
        return false;
    }

    // Convert certificate to PEM format
    BIO* certBio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(certBio, cert);

    char* certBuf = nullptr;
    long certLen = BIO_get_mem_data(certBio, &certBuf);
    std::string certPem(certBuf, certLen);

    // Calculate fingerprint
    std::string actual_fingerprint = CalculateCertificateHash(certPem);
    BIO_free(certBio);

    // If the fingerprint is in our trusted set, allow the connection
    if (auto expected_fingerprint = GetDeviceFingerprint(ip, port); expected_fingerprint) {
        if (*expected_fingerprint == actual_fingerprint) {
            spdlog::info("Certificate fingerprint verified successfully: {}...",
                         actual_fingerprint.substr(0, 8));
            return true;
        } else {
            spdlog::error("Certificate fingerprint mismatch for {}:{}!", ip, port);
            spdlog::error("Expected: {}...", expected_fingerprint->substr(0, 8));
            spdlog::error("Actual: {}...", actual_fingerprint.substr(0, 8));
            spdlog::error("Possible man-in-the-middle attack detected!");
            return false;
        }
    }

    // If allowed unregistered devices, log a warning
    if (unregistered_allowed_) {
        spdlog::warn("Unregistered device {}:{} connected, fingerprint: {}...",
                     ip,
                     port,
                     actual_fingerprint.substr(0, 8));
        return true;
    }
    spdlog::error("No expected fingerprint for {}:{}, rejecting connection", ip, port);
    spdlog::error("Certificate fingerprint: {}...", actual_fingerprint.substr(0, 8));
    return true;
}

void CertificateManager::RegisterDeviceFingerprint(const std::string& ip,
                                                   uint16_t port,
                                                   const std::string& fingerprint) {
    std::string key = makeDeviceKey(ip, port);

    // 如果已存在且不同，记录警告
    auto it = device_fingerprints_.find(key);
    if (it != device_fingerprints_.end() && it->second != fingerprint) {
        spdlog::warn("Device {}:{} fingerprint changed from {} to {}!",
                     ip,
                     port,
                     it->second.substr(0, 8),
                     fingerprint.substr(0, 8));
    }

    device_fingerprints_[key] = fingerprint;
    spdlog::info("Registered fingerprint for {}:{}: {}...", ip, port, fingerprint.substr(0, 8));
}

void CertificateManager::RemoveDeviceFingerprint(const std::string& ip, uint16_t port) {
    std::string key = makeDeviceKey(ip, port);
    auto it = device_fingerprints_.find(key);
    if (it != device_fingerprints_.end()) {
        device_fingerprints_.erase(it);
        spdlog::info("Removed fingerprint for {}:{}", ip, port);
    } else {
        spdlog::warn("No fingerprint found for {}:{}", ip, port);
    }
}

std::optional<std::string> CertificateManager::GetDeviceFingerprint(const std::string& ip,
                                                                    uint16_t port) const {
    std::string key = makeDeviceKey(ip, port);
    auto it = device_fingerprints_.find(key);
    if (it != device_fingerprints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool CertificateManager::generateSelfSignedCertificate() {
    EVP_PKEY* pkey = nullptr;
    X509* x509 = nullptr;

    try {
        // 1. Generate RSA key pair
        spdlog::info("Generating 2048-bit RSA key pair...");
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        if (!ctx || EVP_PKEY_keygen_init(ctx) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize RSA key generation");
        }

        // Set RSA key parameters (key size and public exponent)
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            throw std::runtime_error("Failed to set RSA key size");
        }

        // Generate the key
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            throw std::runtime_error("Failed to generate RSA key pair");
        }

        EVP_PKEY_CTX_free(ctx);

        // 2. Create X509 certificate
        x509 = X509_new();

        // Set version (V3)
        X509_set_version(x509, 2);

        // Set serial number
        ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

        // Set validity period
        X509_gmtime_adj(X509_get_notBefore(x509), 0); // Starting from now
        X509_gmtime_adj(X509_get_notAfter(x509),
                        60 * 60 * 24 * kCertValidityDays); // Valid for 10 years

        // Set public key
        X509_set_pubkey(x509, pkey);

        // Set subject name and issuer name (self-signed)
        X509_NAME* name = X509_get_subject_name(x509);

        char hostname[256];
        gethostname(hostname, sizeof(hostname));

        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*) hostname, -1, -1, 0);
        X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*) "LanSend", -1, -1, 0);
        X509_NAME_add_entry_by_txt(name,
                                   "OU",
                                   MBSTRING_ASC,
                                   (unsigned char*) "Self-Signed",
                                   -1,
                                   -1,
                                   0);

        X509_set_issuer_name(x509, name); // Self is the issuer

        // Sign the certificate
        if (X509_sign(x509, pkey, EVP_sha256()) == 0) {
            throw std::runtime_error("Failed to sign certificate");
        }

        // 3. Convert to PEM format
        BIO* privateBio = BIO_new(BIO_s_mem());
        PEM_write_bio_PrivateKey(privateBio, pkey, nullptr, nullptr, 0, nullptr, nullptr);

        BIO* publicBio = BIO_new(BIO_s_mem());
        PEM_write_bio_PUBKEY(publicBio, pkey);

        BIO* certBio = BIO_new(BIO_s_mem());
        PEM_write_bio_X509(certBio, x509);

        // Read BIO content to string
        char* privateBuf = nullptr;
        long privateLen = BIO_get_mem_data(privateBio, &privateBuf);
        security_context_.private_key_pem = std::string(privateBuf, privateLen);

        char* publicBuf = nullptr;
        long publicLen = BIO_get_mem_data(publicBio, &publicBuf);
        security_context_.public_key_pem = std::string(publicBuf, publicLen);

        char* certBuf = nullptr;
        long certLen = BIO_get_mem_data(certBio, &certBuf);
        security_context_.certificate_pem = std::string(certBuf, certLen);

        // 4. Calculate certificate fingerprint
        security_context_.certificate_hash = CalculateCertificateHash(
            security_context_.certificate_pem);

        // Release all resources
        BIO_free(privateBio);
        BIO_free(publicBio);
        BIO_free(certBio);
        X509_free(x509);
        EVP_PKEY_free(pkey);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Certificate generation error: {}", e.what());

        // Clean up resources
        if (x509)
            X509_free(x509);
        if (pkey)
            EVP_PKEY_free(pkey);

        return false;
    }
}

bool CertificateManager::saveSecurityContext() {
    try {
        std::ofstream privateKeyFile(certificate_dir_ / "private_key.pem");
        privateKeyFile << security_context_.private_key_pem;
        privateKeyFile.close();

        std::ofstream publicKeyFile(certificate_dir_ / "public_key.pem");
        publicKeyFile << security_context_.public_key_pem;
        publicKeyFile.close();

        std::ofstream certFile(certificate_dir_ / "certificate.pem");
        certFile << security_context_.certificate_pem;
        certFile.close();

        std::ofstream fingerprintFile(certificate_dir_ / "fingerprint.txt");
        fingerprintFile << security_context_.certificate_hash;
        fingerprintFile.close();

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save security context: {}", e.what());
        return false;
    }
}

bool CertificateManager::loadSecurityContext() {
    try {
        if (!fs::exists(certificate_dir_ / "private_key.pem")
            || !fs::exists(certificate_dir_ / "public_key.pem")
            || !fs::exists(certificate_dir_ / "certificate.pem")
            || !fs::exists(certificate_dir_ / "fingerprint.txt")) {
            return false;
        }

        std::ifstream privateKeyFile(certificate_dir_ / "private_key.pem");
        std::stringstream privateKeyStream;
        privateKeyStream << privateKeyFile.rdbuf();
        security_context_.private_key_pem = privateKeyStream.str();

        std::ifstream publicKeyFile(certificate_dir_ / "public_key.pem");
        std::stringstream publicKeyStream;
        publicKeyStream << publicKeyFile.rdbuf();
        security_context_.public_key_pem = publicKeyStream.str();

        std::ifstream certFile(certificate_dir_ / "certificate.pem");
        std::stringstream certStream;
        certStream << certFile.rdbuf();
        security_context_.certificate_pem = certStream.str();

        std::ifstream fingerprintFile(certificate_dir_ / "fingerprint.txt");
        std::stringstream fingerprintStream;
        fingerprintStream << fingerprintFile.rdbuf();
        security_context_.certificate_hash = fingerprintStream.str();

        return !security_context_.private_key_pem.empty()
               && !security_context_.public_key_pem.empty()
               && !security_context_.certificate_pem.empty()
               && !security_context_.certificate_hash.empty();
    } catch (const std::exception& e) {
        spdlog::error("Failed to load security context: {}", e.what());
        return false;
    }
}

std::string CertificateManager::makeDeviceKey(const std::string& ip, uint16_t port) const {
    return std::format("{}:{}", ip, port);
}

} // namespace lansend::core