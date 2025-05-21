#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <core/security/open_ssl_provider.h>
#include <exception>
#include <spdlog/spdlog.h>

namespace ssl = boost::asio::ssl;
namespace uuids = boost::uuids;

namespace lansend::core {

OpenSSLProvider::~OpenSSLProvider() {
    if (initialized_) {
        // Though OpenSSL will automatically clean up since 1.1.0,
        // keeping this for backward compatibility
        EVP_cleanup();
        ERR_free_strings();
        CRYPTO_cleanup_all_ex_data();
        initialized_ = false;
    }
}

OpenSSLProvider& OpenSSLProvider::instance() {
    static OpenSSLProvider instance;
    return instance;
}

std::string OpenSSLProvider::createSessionId(std::string_view prefix) {
    uuids::random_generator gen;
    uuids::uuid id = gen();
    std::string uuid_str = uuids::to_string(id);
    return std::string(prefix) + "_" + uuid_str;
}

void OpenSSLProvider::InitOpenSSL() {
    if (!instance().initialized_) {
        if (SSL_library_init() < 0) {
            spdlog::error("SSL_library_init failed");
            std::terminate();
        }
        if (SSL_load_error_strings() < 0) {
            spdlog::error("SSL_load_error_strings failed");
            std::terminate();
        }
        if (OpenSSL_add_all_algorithms() < 0) {
            spdlog::error("OpenSSL_add_all_algorithms failed");
            std::terminate();
        }
        if (ERR_load_crypto_strings() < 0) {
            spdlog::error("ERR_load_crypto_strings failed");
            std::terminate();
        }
        instance().initialized_ = true;
    }
}

ssl::context OpenSSLProvider::BuildClientContext(
    std::function<bool(bool, ssl::verify_context&)> verify_callback) {
    ssl::context ctx(ssl::context::tlsv12_client);

    ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2
                    | ssl::context::no_sslv3);

    SSL_CTX_set_session_cache_mode(ctx.native_handle(), SSL_SESS_CACHE_CLIENT);
    SSL_CTX_sess_set_cache_size(ctx.native_handle(), 128);

    std::string session_id_context = createSessionId("lansend_client");
    SSL_CTX_set_session_id_context(ctx.native_handle(),
                                   reinterpret_cast<const unsigned char*>(
                                       session_id_context.c_str()),
                                   session_id_context.length());

    ctx.set_verify_mode(ssl::verify_peer);
    ctx.set_verify_callback(verify_callback);

    return ctx;
}

ssl::context OpenSSLProvider::BuildServerContext(std::string_view cert_pem,
                                                 std::string_view key_pem) {
    ssl::context ctx(ssl::context::tlsv12_server);

    ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2
                    | ssl::context::no_sslv3 | ssl::context::single_dh_use);

    ctx.use_certificate(boost::asio::buffer(cert_pem), ssl::context::pem);
    ctx.use_private_key(boost::asio::buffer(key_pem), ssl::context::pem);

    return ctx;
}

bool OpenSSLProvider::SetHostname(SSL* ssl, std::string_view hostname) {
    if (!ssl || !SSL_set_tlsext_host_name(ssl, hostname.data())) {
        return false;
    }
    return true;
}

} // namespace lansend::core