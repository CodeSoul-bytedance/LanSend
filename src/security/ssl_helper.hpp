#pragma once

#include <boost/asio/ssl/context.hpp>
#include <functional>
#include <string>

namespace ssl = boost::asio::ssl;

class SSLHelper {
public:
    using VerifyCallback = std::function<bool(bool, ssl::verify_context&)>;

    static ssl::context create_client_context(VerifyCallback verify_callback);

    static ssl::context create_server_context(const std::string& cert_pem,
                                              const std::string& key_pem);

    static bool set_hostname(SSL* ssl, const std::string& hostname);

    static void enable_session_cache(ssl::context& ctx, const std::string& session_id);

    static bool is_session_reused(SSL* ssl);
    static std::string get_cipher_info(SSL* ssl);

    static std::string create_session_id(const std::string& prefix);
};