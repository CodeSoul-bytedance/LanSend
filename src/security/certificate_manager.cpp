#include "certificate_manager.hpp"
#include <fstream>
#include <iomanip>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <vector>

CertificateManager::CertificateManager()
    : ssl_context_(
          std::make_unique<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server)) {
    ssl_context_->set_options(
        boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::no_tlsv1
        | boost::asio::ssl::context::no_tlsv1_1);
}

CertificateManager::~CertificateManager() {}

boost::asio::ssl::context& CertificateManager::get_ssl_context() {
    return *ssl_context_;
}

bool CertificateManager::load_certificate(const std::filesystem::path& cert_path,
                                          const std::filesystem::path& key_path) {
    if (!std::filesystem::exists(cert_path) || !std::filesystem::exists(key_path)) {
        spdlog::error("Certificate or key file not found. Cert: '{}', Key: '{}'",
                      cert_path.string(),
                      key_path.string());
        return false;
    }

    try {
        ssl_context_->use_certificate_chain_file(cert_path.string());
        ssl_context_->use_private_key_file(key_path.string(), boost::asio::ssl::context::pem);
        spdlog::info("Certificate and private key loaded successfully. Cert: '{}', Key: '{}'",
                     cert_path.string(),
                     key_path.string());

        if (!calculate_fingerprint()) {
            spdlog::error("Failed to calculate certificate fingerprint.");
            return false;
        }
        return true;
    } catch (const boost::system::system_error& e) {
        spdlog::error("Failed to load certificate or private key: {}", e.what());
        return false;
    }
}

bool CertificateManager::calculate_fingerprint() {
    SSL_CTX* ctx = ssl_context_->native_handle();
    if (!ctx) {
        spdlog::error("SSL_CTX native handle is null.");
        return false;
    }

    X509* cert = SSL_CTX_get0_certificate(ctx);

    if (!cert) {
        spdlog::error(
            "Could not get X509 certificate from SSL_CTX (SSL_CTX_get0_certificate failed).");
        return false;
    }

    unsigned char fingerprint[EVP_MAX_MD_SIZE];
    unsigned int fingerprint_len;

    // X509_digest 函数计算 DER 编码证书的摘要。
    if (X509_digest(cert, EVP_sha256(), fingerprint, &fingerprint_len) == 0) {
        spdlog::error("X509_digest failed to calculate SHA256 fingerprint.");
        return false;
    }

    std::stringstream ss;
    for (unsigned int i = 0; i < fingerprint_len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(fingerprint[i]);
    }
    certificate_fingerprint_ = ss.str();
    spdlog::info("Calculated certificate fingerprint: {}", certificate_fingerprint_);

    return true;
}

std::string CertificateManager::get_certificate_fingerprint() const {
    return certificate_fingerprint_;
}

static bool add_ext(X509* cert, int nid, const char* value) {
    X509_EXTENSION* ex;
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
    ex = X509V3_EXT_nconf_nid(nullptr, &ctx, nid, (char*) value);
    if (!ex) {
        return false;
    }
    X509_add_ext(cert, ex, -1);
    X509_EXTENSION_free(ex);
    return true;
}

bool CertificateManager::generate_self_signed_certificate(const std::string& common_name,
                                                          const std::filesystem::path& cert_path,
                                                          const std::filesystem::path& key_path,
                                                          int key_length_bits,
                                                          int expiry_days) {
    spdlog::info("Generating self-signed certificate: CN={}, CertPath='{}', KeyPath='{}'",
                 common_name,
                 cert_path.string(),
                 key_path.string());

    bool success = false;
    EVP_PKEY* pkey = nullptr;
    X509* x509 = nullptr;
    RSA* rsa = nullptr;
    BIGNUM* bne = nullptr;
    FILE* key_file = nullptr;
    FILE* cert_file = nullptr;
    X509_NAME* name = nullptr;

    do {
        // 1. Generate RSA key pair
        pkey = EVP_PKEY_new();
        if (!pkey) {
            spdlog::error("EVP_PKEY_new failed.");
            break;
        }

        rsa = RSA_new();
        if (!rsa) {
            spdlog::error("RSA_new failed.");
            break;
        }

        bne = BN_new();
        if (!bne || !BN_set_word(bne, RSA_F4)) {
            spdlog::error("BN_new or BN_set_word failed.");
            break;
        }

        if (!RSA_generate_key_ex(rsa, key_length_bits, bne, nullptr)) {
            spdlog::error("RSA_generate_key_ex failed.");
            break;
        }

        if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
            spdlog::error("EVP_PKEY_assign_RSA failed.");
            break;
        }
        rsa = nullptr;

        // 2. Create X509 certificate structure
        x509 = X509_new();
        if (!x509) {
            spdlog::error("X509_new failed.");
            break;
        }

        // 3. Set certificate version (X509v3)
        X509_set_version(x509, 2);

        // 4. Set serial number
        ASN1_INTEGER_set(X509_get_serialNumber(x509), (long) time(nullptr));

        // 5. Set validity period
        X509_gmtime_adj(X509_get_notBefore(x509), 0);
        X509_gmtime_adj(X509_get_notAfter(x509), (long) 60 * 60 * 24 * expiry_days);

        // 6. Set public key
        if (X509_set_pubkey(x509, pkey) != 1) {
            spdlog::error("X509_set_pubkey failed.");
            break;
        }

        // 7. Set issuer and subject name (self-signed, so they are the same)
        name = X509_get_subject_name(x509);
        if (!name) {
            spdlog::error("X509_get_subject_name failed.");
            break;
        }
        if (X509_NAME_add_entry_by_txt(
                name, "CN", MBSTRING_ASC, (const unsigned char*) common_name.c_str(), -1, -1, 0)
            != 1) {
            spdlog::error("X509_NAME_add_entry_by_txt for CN failed.");
            break;
        }

        if (X509_set_issuer_name(x509, name) != 1) { // Issuer is the same as subject
            spdlog::error("X509_set_issuer_name failed.");
            break;
        }

        // 8. Add X509v3 extensions
        if (!add_ext(x509, NID_basic_constraints, "critical,CA:FALSE")) {
            spdlog::error("Failed to add basic_constraints extension.");
            break;
        }
        if (!add_ext(x509, NID_key_usage, "critical,digitalSignature,keyEncipherment")) {
            spdlog::error("Failed to add key_usage extension.");
            break;
        }
        if (!add_ext(x509, NID_subject_key_identifier, "hash")) {
            spdlog::error("Failed to add subject_key_identifier extension.");
            break;
        }

        // 9. Sign the certificate
        if (X509_sign(x509, pkey, EVP_sha256()) == 0) {
            spdlog::error("X509_sign failed.");
            break;
        }

        // 10. Write private key to PEM file
#ifdef _WIN32
        if (_wfopen_s(&key_file, key_path.wstring().c_str(), L"wb") != 0 || !key_file) {
#else
        key_file = fopen(key_path.c_str(), "wb");
        if (!key_file) {
#endif
            spdlog::error("Failed to open key file for writing: {}", key_path.string());
            break;
        }
        if (PEM_write_PrivateKey(key_file, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
            spdlog::error("PEM_write_PrivateKey failed.");
            break;
        }
        fclose(key_file);
        key_file = nullptr;

        // 11. Write certificate to PEM file
#ifdef _WIN32
        if (_wfopen_s(&cert_file, cert_path.wstring().c_str(), L"wb") != 0 || !cert_file) {
#else
        cert_file = fopen(cert_path.c_str(), "wb");
        if (!cert_file) {
#endif
            spdlog::error("Failed to open certificate file for writing: {}", cert_path.string());
            break;
        }
        if (PEM_write_X509(cert_file, x509) != 1) {
            spdlog::error("PEM_write_X509 failed.");
            // cert_file will be closed in cleanup
            break;
        }
        fclose(cert_file);
        cert_file = nullptr;

        spdlog::info("Self-signed certificate and key generated successfully.");
        success = true;

    } while (false);

    // Cleanup section
    if (key_file)
        fclose(key_file);
    if (cert_file)
        fclose(cert_file);
    if (x509)
        X509_free(x509);
    if (pkey)
        EVP_PKEY_free(pkey);
    if (rsa)
        RSA_free(rsa);
    if (bne)
        BN_free(bne);

    if (success) {
        // 12. Load the newly generated certificate
        return load_certificate(cert_path, key_path);
    }

    return false;
}