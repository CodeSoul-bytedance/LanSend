#include <core/security/certificate_manager.h>
#include <core/security/open_ssl_provider.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

namespace fs = std::filesystem;
namespace ssl = boost::asio::ssl;
using json = nlohmann::json;

namespace lansend::core {

CertificateManager::CertificateManager(const fs::path& certDir)
    : certificate_dir_(certDir) {
    if (!fs::exists(certDir)) {
        fs::create_directories(certDir);
    }

    OpenSSLProvider::InitOpenSSL();
    initSecurityContext();

    trusted_fingerprints_path_ = certificate_dir_ / "trusted_fingerprints.json";
    loadTrustedFingerprints();
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

bool CertificateManager::VerifyCertificate(bool preverified, ssl::verify_context& ctx) {
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
    std::string actualFingerprint = CalculateCertificateHash(certPem);

    BIO_free(certBio);

    // Check if the fingerprint is in our trusted set
    if (trusted_fingerprints_.find(actualFingerprint) != trusted_fingerprints_.end()) {
        // Known fingerprint, connection is trusted
        spdlog::info("Certificate fingerprint verified successfully: {}", actualFingerprint);
        return true;
    } else {
        // First connection with this fingerprint, prompt user to trust it
        spdlog::warn("New certificate fingerprint detected: {}", actualFingerprint);

        // In a more official application, this might show a prompt to the user
        // For now, we'll auto-trust on first encounter
        trusted_fingerprints_.insert(actualFingerprint);
        saveTrustedFingerprints();

        spdlog::info("Automatically trusted new fingerprint: {}", actualFingerprint);
        return true;
    }
}

void CertificateManager::TrustFingerprint(const std::string& fingerprint) {
    trusted_fingerprints_.insert(fingerprint);
    saveTrustedFingerprints();
    spdlog::info("Added fingerprint to trusted list: {}", fingerprint);
}

bool CertificateManager::IsFingerprintTrusted(const std::string& fingerprint) {
    return trusted_fingerprints_.find(fingerprint) != trusted_fingerprints_.end();
}

bool CertificateManager::TrustCertificate(const std::string& certificatePem) {
    try {
        std::string fingerprint = CalculateCertificateHash(certificatePem);
        TrustFingerprint(fingerprint);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to add certificate to trust list: {}", e.what());
        return false;
    }
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

        // Add our own fingerprint to trusted list
        trusted_fingerprints_.insert(security_context_.certificate_hash);
        saveTrustedFingerprints();

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

        // Make sure our own fingerprint is trusted
        trusted_fingerprints_.insert(security_context_.certificate_hash);
        saveTrustedFingerprints();

        return !security_context_.private_key_pem.empty()
               && !security_context_.public_key_pem.empty()
               && !security_context_.certificate_pem.empty()
               && !security_context_.certificate_hash.empty();
    } catch (const std::exception& e) {
        spdlog::error("Failed to load security context: {}", e.what());
        return false;
    }
}

void CertificateManager::loadTrustedFingerprints() {
    if (!fs::exists(trusted_fingerprints_path_)) {
        spdlog::info("No trusted fingerprints file found, will create on first connection");
        return;
    }

    try {
        std::ifstream file(trusted_fingerprints_path_);
        json j = json::parse(file);

        if (j.is_array()) {
            for (const auto& fingerprint : j) {
                trusted_fingerprints_.insert(fingerprint.get<std::string>());
            }
        }

        spdlog::info("Loaded {} trusted fingerprints", trusted_fingerprints_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to load trusted fingerprints: {}", e.what());
    }
}

void CertificateManager::saveTrustedFingerprints() {
    try {
        json j = json::array();
        for (const auto& fingerprint : trusted_fingerprints_) {
            j.push_back(fingerprint);
        }

        std::ofstream file(trusted_fingerprints_path_);
        file << j.dump(2);
        file.close();

        spdlog::debug("Saved {} trusted fingerprints", trusted_fingerprints_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to save trusted fingerprints: {}", e.what());
    }
}

} // namespace lansend::core