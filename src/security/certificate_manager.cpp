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

using json = nlohmann::json;
namespace fs = std::filesystem;

CertificateManager::CertificateManager(const fs::path& cert_dir) {
    if (!fs::exists(cert_dir)) {
        fs::create_directories(cert_dir);
    }

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
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

std::string CertificateManager::calculate_certificate_hash(const std::string& certificate_pem) {
    // Use SHA-256 to hash the PEM certificate
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    // Create EVP_MD_CTX and initialize it
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, certificate_pem.c_str(), certificate_pem.size());
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    // Cast to hex string
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int) hash[i];
    }

    return ss.str();
}

bool CertificateManager::verify_remote_certificate(X509* cert,
                                                   const std::string& expected_fingerprint) {
    if (!cert)
        return false;

    // Cast to PEM format
    BIO* cert_bio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(cert_bio, cert);

    char* cert_buf = nullptr;
    long cert_len = BIO_get_mem_data(cert_bio, &cert_buf);
    std::string cert_pem(cert_buf, cert_len);

    std::string actual_fingerprint = calculate_certificate_hash(cert_pem);

    BIO_free(cert_bio);

    bool match = (actual_fingerprint == expected_fingerprint);
    if (match) {
        spdlog::info("Certificate fingerprint verified successfully");
    } else {
        spdlog::warn("Certificate fingerprint mismatch! Expected: {}, Got: {}",
                     expected_fingerprint,
                     actual_fingerprint);
    }

    return match;
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
        // Generate RSA key pair
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

        // Create a new X509 certificate
        x509 = X509_new();

        // Set version
        X509_set_version(x509, 2);

        // Set serial number
        ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

        // Set validity period
        X509_gmtime_adj(X509_get_notBefore(x509), 0);                               // from now
        X509_gmtime_adj(X509_get_notAfter(x509), 60 * 60 * 24 * kCertValidityDays); // 10 years

        // Set public key
        X509_set_pubkey(x509, pkey);

        // Set subject name and issuer name
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

        X509_set_issuer_name(x509, name); // self-signed, so issuer is the same as subject

        // Sign the certificate
        if (X509_sign(x509, pkey, EVP_sha256()) == 0) {
            throw std::runtime_error("Failed to sign certificate");
        }

        // Cast to PEM format
        BIO* private_bio = BIO_new(BIO_s_mem());
        PEM_write_bio_PrivateKey(private_bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);

        BIO* public_bio = BIO_new(BIO_s_mem());
        PEM_write_bio_PUBKEY(public_bio, pkey);

        BIO* cert_bio = BIO_new(BIO_s_mem());
        PEM_write_bio_X509(cert_bio, x509);

        // Read BIO data into strings
        char* private_buf = nullptr;
        long private_len = BIO_get_mem_data(private_bio, &private_buf);
        security_context_.private_key_pem = std::string(private_buf, private_len);

        char* public_buf = nullptr;
        long public_len = BIO_get_mem_data(public_bio, &public_buf);
        security_context_.public_key_pem = std::string(public_buf, public_len);

        char* cert_buf = nullptr;
        long cert_len = BIO_get_mem_data(cert_bio, &cert_buf);
        security_context_.certificate_pem = std::string(cert_buf, cert_len);

        security_context_.certificate_hash = calculate_certificate_hash(
            security_context_.certificate_pem);
        // Clean up
        BIO_free(private_bio);
        BIO_free(public_bio);
        BIO_free(cert_bio);
        X509_free(x509);
        EVP_PKEY_free(pkey);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Certificate generation error: {}", e.what());

        // Clean up
        if (x509)
            X509_free(x509);
        if (pkey)
            EVP_PKEY_free(pkey);

        return false;
    }
}

bool CertificateManager::save_security_context() {
    try {
        std::ofstream private_key_file(certificate_dir_ / "private_key.pem");
        private_key_file << security_context_.private_key_pem;
        private_key_file.close();

        std::ofstream public_key_file(certificate_dir_ / "public_key.pem");
        public_key_file << security_context_.public_key_pem;
        public_key_file.close();

        std::ofstream cert_file(certificate_dir_ / "certificate.pem");
        cert_file << security_context_.certificate_pem;
        cert_file.close();

        std::ofstream fingerprint_file(certificate_dir_ / "fingerprint.txt");
        fingerprint_file << security_context_.certificate_hash;
        fingerprint_file.close();

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

        std::ifstream private_key_file(certificate_dir_ / "private_key.pem");
        std::stringstream private_key_stream;
        private_key_stream << private_key_file.rdbuf();
        security_context_.private_key_pem = private_key_stream.str();

        std::ifstream public_key_file(certificate_dir_ / "public_key.pem");
        std::stringstream public_key_stream;
        public_key_stream << public_key_file.rdbuf();
        security_context_.public_key_pem = public_key_stream.str();

        std::ifstream cert_file(certificate_dir_ / "certificate.pem");
        std::stringstream cert_stream;
        cert_stream << cert_file.rdbuf();
        security_context_.certificate_pem = cert_stream.str();

        std::ifstream fingerprint_file(certificate_dir_ / "fingerprint.txt");
        std::stringstream fingerprint_stream;
        fingerprint_stream << fingerprint_file.rdbuf();
        security_context_.certificate_hash = fingerprint_stream.str();

        return !security_context_.private_key_pem.empty()
               && !security_context_.public_key_pem.empty()
               && !security_context_.certificate_pem.empty()
               && !security_context_.certificate_hash.empty();
    } catch (const std::exception& e) {
        spdlog::error("Failed to load security context: {}", e.what());
        return false;
    }
}