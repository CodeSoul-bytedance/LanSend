#include "certificate_manager.hpp"
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace fs = std::filesystem;
namespace ssl = boost::asio::ssl;
using json = nlohmann::json;

CertificateManager::CertificateManager(const fs::path& certDir)
    : certificate_dir_(certDir) {
    if (!fs::exists(certDir)) {
        fs::create_directories(certDir);
    }

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    trusted_fingerprints_path_ = certificate_dir_ / "trusted_hosts.json";
    load_trusted_fingerprints();
}

CertificateManager::~CertificateManager() {
    EVP_cleanup();
    ERR_free_strings();
}

bool CertificateManager::init_security_context() {
    if (load_security_context()) {
        spdlog::info("Loaded existing certificate with fingerprint: {}",
                     security_context_.certificate_hash);
        return true;
    }

    if (generate_self_signed_certificate()) {
        spdlog::info("Generated new self-signed certificate with fingerprint: {}",
                     security_context_.certificate_hash);
        return save_security_context();
    }

    return false;
}

const SecurityContext& CertificateManager::security_context() const {
    return security_context_;
}

// Calculate certificate fingerprint using SHA-256
std::string CertificateManager::calculate_certificate_hash(const std::string& certificatePem) {
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

bool CertificateManager::verify_certificate(bool preverified, ssl::verify_context& ctx) {
    // Get the certificate being verified
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    if (!cert) {
        spdlog::error("No certificate to verify");
        return false;
    }

    // Get the hostname currently being connected to
    std::string hostname;
    {
        std::lock_guard<std::mutex> lock(hostname_mutex_);
        hostname = current_hostname_;
    }

    if (hostname.empty()) {
        spdlog::error("No hostname set for verification");
        return false;
    }

    // Convert certificate to PEM format
    BIO* certBio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(certBio, cert);

    char* certBuf = nullptr;
    long certLen = BIO_get_mem_data(certBio, &certBuf);
    std::string certPem(certBuf, certLen);

    // Calculate fingerprint
    std::string actualFingerprint = calculate_certificate_hash(certPem);

    BIO_free(certBio);

    // Check if the host already has a trusted fingerprint
    if (trusted_hosts_.find(hostname) != trusted_hosts_.end()) {
        // Known host, verify if fingerprint matches
        std::string expectedFingerprint = trusted_hosts_[hostname];

        if (actualFingerprint == expectedFingerprint) {
            spdlog::info("Certificate fingerprint verified successfully for {}", hostname);
            return true;
        } else {
            spdlog::error("SECURITY ALERT: Certificate fingerprint mismatch for {}!", hostname);
            spdlog::error("Expected: {}", expectedFingerprint);
            spdlog::error("Received: {}", actualFingerprint);
            spdlog::error("This could indicate a man-in-the-middle attack!");

            // In a real application, a user confirmation mechanism could be provided
            return false;
        }
    } else {
        // First connection to this host, record fingerprint
        spdlog::warn("First connection to {}, saving fingerprint: {}", hostname, actualFingerprint);
        trusted_hosts_[hostname] = actualFingerprint;
        save_trusted_fingerprints();
        return true;
    }
}

void CertificateManager::trust_host(const std::string& hostname, const std::string& fingerprint) {
    trusted_hosts_[hostname] = fingerprint;
    save_trusted_fingerprints();
}

bool CertificateManager::is_host_trusted(const std::string& hostname,
                                         const std::string& fingerprint) {
    auto it = trusted_hosts_.find(hostname);
    if (it != trusted_hosts_.end()) {
        return it->second == fingerprint;
    }
    return false;
}

void CertificateManager::set_current_hostname(const std::string& hostname) {
    std::lock_guard<std::mutex> lock(hostname_mutex_);
    current_hostname_ = hostname;
}

std::string CertificateManager::current_hostname() const {
    std::lock_guard<std::mutex> lock(hostname_mutex_);
    return current_hostname_;
}

bool CertificateManager::generate_self_signed_certificate() {
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
        X509_NAME_add_entry_by_txt(name,
                                   "O",
                                   MBSTRING_ASC,
                                   (unsigned char*) "P2P File Transfer App",
                                   -1,
                                   -1,
                                   0);
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
        security_context_.certificate_hash = calculate_certificate_hash(
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

bool CertificateManager::save_security_context() {
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

bool CertificateManager::load_security_context() {
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

void CertificateManager::load_trusted_fingerprints() {
    if (!fs::exists(trusted_fingerprints_path_)) {
        spdlog::info("No trusted fingerprints file found, will create on first connection");
        return;
    }

    try {
        std::ifstream file(trusted_fingerprints_path_);
        json j = json::parse(file);

        for (const auto& [host, fingerprint] : j.items()) {
            trusted_hosts_[host] = fingerprint.get<std::string>();
        }

        spdlog::info("Loaded {} trusted host fingerprints", trusted_hosts_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to load trusted fingerprints: {}", e.what());
    }
}

void CertificateManager::save_trusted_fingerprints() {
    try {
        json j;
        for (const auto& [host, fingerprint] : trusted_hosts_) {
            j[host] = fingerprint;
        }

        std::ofstream file(trusted_fingerprints_path_);
        file << j.dump(2);
        file.close();

        spdlog::debug("Saved {} trusted host fingerprints", trusted_hosts_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to save trusted fingerprints: {}", e.what());
    }
}