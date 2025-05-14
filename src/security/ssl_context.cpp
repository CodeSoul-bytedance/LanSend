#include "ssl_context.hpp"
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>

ssl::context SSLContext::client_context(VerifyCallback verify_callback) {
    ssl::context ctx(ssl::context::tlsv12_client);

    ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2
                    | ssl::context::no_sslv3);

    SSL_CTX_set_session_cache_mode(ctx.native_handle(), SSL_SESS_CACHE_CLIENT);

    SSL_CTX_sess_set_cache_size(ctx.native_handle(), 128);

    std::string session_id_context = create_session_id("lansend_client");
    SSL_CTX_set_session_id_context(ctx.native_handle(),
                                   reinterpret_cast<const unsigned char*>(
                                       session_id_context.c_str()),
                                   session_id_context.length());

    ctx.set_verify_mode(ssl::verify_peer);
    ctx.set_verify_callback(verify_callback);

    return ctx;
}

ssl::context SSLContext::server_context(const std::string& cert_pem, const std::string& key_pem) {
    ssl::context ctx(ssl::context::tlsv12_server);

    ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2
                    | ssl::context::no_sslv3 | ssl::context::single_dh_use);

    ctx.use_certificate(boost::asio::buffer(cert_pem), ssl::context::pem);
    ctx.use_private_key(boost::asio::buffer(key_pem), ssl::context::pem);

    SSL_CTX_set_session_cache_mode(ctx.native_handle(),
                                   SSL_SESS_CACHE_SERVER | SSL_SESS_CACHE_NO_INTERNAL_STORE);

    SSL_CTX_sess_set_cache_size(ctx.native_handle(), 1024);

    std::string session_id_context = create_session_id("lansend_server");
    SSL_CTX_set_session_id_context(ctx.native_handle(),
                                   reinterpret_cast<const unsigned char*>(
                                       session_id_context.c_str()),
                                   session_id_context.length());

    return ctx;
}

bool SSLContext::set_hostname(SSL* ssl, const std::string& hostname) {
    if (!ssl)
        return false;

    if (!SSL_set_tlsext_host_name(ssl, hostname.c_str())) {
        return false;
    }

    return true;
}

void SSLContext::enable_session_cache(ssl::context& ctx, const std::string& session_id) {
    SSL_CTX_set_session_cache_mode(ctx.native_handle(), SSL_SESS_CACHE_CLIENT);

    SSL_CTX_sess_set_cache_size(ctx.native_handle(), 128);

    SSL_CTX_set_session_id_context(ctx.native_handle(),
                                   reinterpret_cast<const unsigned char*>(session_id.c_str()),
                                   session_id.length());
}

bool SSLContext::is_session_reused(SSL* ssl) {
    if (!ssl)
        return false;

    return SSL_session_reused(ssl) != 0;
}

std::string SSLContext::get_cipher_info(SSL* ssl) {
    if (!ssl)
        return "Unknown";

    const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl);
    if (!cipher)
        return "Unknown";

    return SSL_CIPHER_get_name(cipher);
}

std::string SSLContext::create_session_id(const std::string& prefix) {
    boost::uuids::random_generator gen;
    boost::uuids::uuid id = gen();
    std::string uuid_str = boost::uuids::to_string(id);

    return prefix + "_" + uuid_str;
}