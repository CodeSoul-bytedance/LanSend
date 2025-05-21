/**
 * @file open_ssl.h
 * @brief providing OpenSSL headers, OpenSSL library initialization and common connection operations
 */
#pragma once

#include <boost/asio/ssl/context.hpp>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <string_view>

namespace lansend::core {

class OpenSSLProvider {
private:
    OpenSSLProvider() = default;
    OpenSSLProvider(const OpenSSLProvider&) = delete;
    OpenSSLProvider& operator=(const OpenSSLProvider&) = delete;
    ~OpenSSLProvider();

    static OpenSSLProvider& instance();
    static std::string createSessionId(std::string_view prefix);

    bool initialized_ = false;

public:
    /**
     * @brief Initialize OpenSSL library
     * 
     * This function should be called before using any OpenSSL functions.
     * It will only initialize once, can be called multiple times safely.
     */
    static void InitOpenSSL();

    /**
     * @brief Build a client SSL context with the given verification callback
     * 
     * @param verify_callback Callback function called during the certificate verification process
     * @return boost::asio::ssl::context SSL context configured for client use
     */
    static boost::asio::ssl::context BuildClientContext(
        std::function<bool(bool, boost::asio::ssl::verify_context&)> verify_callback);

    /**
     * @brief Build a server SSL context with the given certificate and key in PEM format
     * 
     * @param cert_pem Certificate in PEM format
     * @param key_pem Private key in PEM format
     * @return boost::asio::ssl::context SSL context configured for server use
     */
    static boost::asio::ssl::context BuildServerContext(std::string_view cert_pem,
                                                        std::string_view key_pem);

    /**
     * @brief Set the hostname for the SSL connection
     * 
     * @param ssl SSL connection object
     * @param hostname Hostname to set
     * @return bool True if hostname was set successfully, false otherwise
     */
    static bool SetHostname(SSL* ssl, std::string_view hostname);
};

} // namespace lansend::core