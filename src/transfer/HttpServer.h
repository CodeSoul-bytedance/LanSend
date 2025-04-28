#include "../security/certificate_manager.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <map>
#include <string>

namespace http = boost::beast::http;

class HttpServer {
public:
    HttpServer(boost::asio::io_context& ioc, CertificateManager& cm);
    ~HttpServer();

    boost::asio::io_context& io_context_;
    boost::asio::ssl::context ssl_context_;
    CertificateManager& cert_manager_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::map<std::string, RouteHandler> routes_;
    using RouteHandler = std::function<boost::asio::awaitable<http::response<http::string_body>>(
        http::request<http::string_body>)>;
};