#include "network_manager.hpp"

NetworkManager::NetworkManager(boost::asio::io_context& ioc, Config& config)
    : io_context_(ioc)
    , config_(config)
    , logger_(Logger::get_instance()) {}

NetworkManager::~NetworkManager() {
    stop();
}

void NetworkManager::start(uint16_t port) {
    logger_.info("Starting NetworkManager...");
    cert_manager_->generate_self_signed_certificate("cert.pem", "key.pem");
    server_->start(port);
    discovery_manager_->start(port);
    logger_.info("NetworkManager started.");
}

void NetworkManager::stop() {
    logger_.info("Stopping NetworkManager...");
    server_->stop();
    discovery_manager_->stop();
    logger_.info("NetworkManager stopped.");
}