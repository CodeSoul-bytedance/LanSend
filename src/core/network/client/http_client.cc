#include <core/network/client/http_client.h>
#include <core/security/open_ssl_provider.h>
#include <spdlog/spdlog.h>

namespace lansend::core {

HttpsClient::HttpsClient(net::io_context& ioc, CertificateManager& cert_manager)
    : ioc_(ioc)
    , cert_manager_(cert_manager)
    , ssl_ctx_(OpenSSLProvider::BuildClientContext(
          [&](bool preverified, ssl::verify_context& ctx) -> bool {
              return cert_manager.VerifyCertificate(preverified, ctx);
          })) {}

HttpsClient::~HttpsClient() {
    if (ssl_session_) {
        SSL_SESSION_free(ssl_session_);
        ssl_session_ = nullptr;
    }
}

net::awaitable<bool> HttpsClient::Connect(std::string_view host, unsigned short port) {
    try {
        if (connection_) {
            co_await Disconnect();
        }

        connection_ = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(beast::tcp_stream(ioc_),
                                                                             ssl_ctx_);

        if (!OpenSSLProvider::SetHostname(connection_->native_handle(), host)) {
            throw std::runtime_error("Failed to set SNI Hostname");
        }

        tcp::resolver resolver(ioc_);
        auto results = co_await resolver.async_resolve(host, std::to_string(port));

        beast::get_lowest_layer(*connection_).expires_after(std::chrono::seconds(30));
        co_await beast::get_lowest_layer(*connection_).async_connect(results);
        beast::get_lowest_layer(*connection_).expires_never();

        co_await connection_->async_handshake(ssl::stream_base::client);

        if (ssl_session_) {
            SSL_SESSION_free(ssl_session_);
        }
        ssl_session_ = SSL_get1_session(connection_->native_handle());

        current_host_ = host;
        current_port_ = port;

        spdlog::info("Connected to {}:{}", host, port);
        co_return true;
    } catch (const std::exception& e) {
        spdlog::error("Connection error: {}", e.what());
        connection_.reset();
        current_host_.clear();
        current_port_ = 0;

        co_return false;
    }
}

net::awaitable<bool> HttpsClient::Disconnect() {
    if (!connection_) {
        co_return true;
    }

    try {
        beast::error_code ec;
        connection_->shutdown(ec);

        if (ec && ec != net::error::eof) {
            spdlog::debug("SSL shutdown notice: {}", ec.message());
        }

        connection_.reset();
        current_host_.clear();
        current_port_ = 0;

        spdlog::debug("Disconnected");
        co_return true;
    } catch (const std::exception& e) {
        spdlog::error("Disconnect error: {}", e.what());
        connection_.reset();
        current_host_.clear();
        current_port_ = 0;

        co_return false;
    }
}

bool HttpsClient::IsConnected() const {
    return connection_ != nullptr;
}

std::string HttpsClient::current_host() const {
    return current_host_;
}

unsigned short HttpsClient::current_port() const {
    return current_port_;
}

std::optional<boost::asio::ip::tcp::endpoint> HttpsClient::local_endpoint() const {
    if (!connection_) {
        return std::nullopt;
    }

    try {
        return beast::get_lowest_layer(*connection_).socket().local_endpoint();
    } catch (const std::exception& e) {
        spdlog::error("Failed to get connection endpoint: {}", e.what());
        return std::nullopt;
    }
}

} // namespace lansend::core