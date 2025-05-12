#include "network_manager.hpp"
#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <filesystem>

NetworkManager::NetworkManager(boost::asio::io_context& ioc, Config& config)
    : io_context_(ioc)
    , cert_manager_(std::make_unique<CertificateManager>())
    , server_(std::make_unique<lansend::api::HttpServer>(ioc, cert_manager_->get_ssl_context()))
    , config_(config)
    , logger_(Logger::get_instance())
    , discovery_manager_(std::make_unique<DiscoveryManager>(ioc))
    , transfer_manager_(std::make_unique<TransferManager>(ioc)) {
    // Initialize callbacks to nullptr
    device_found_callback_ = nullptr;
    transfer_progress_callback_ = nullptr;
    transfer_complete_callback_ = nullptr;
}

NetworkManager::~NetworkManager() {
    stop();
}

void NetworkManager::start(uint16_t port) {
    logger_.info("Starting NetworkManager...");

    // Generate self-signed certificate (or load if exists)
    std::filesystem::path cert_path = "cert.pem";
    std::filesystem::path key_path = "key.pem";

    if (!std::filesystem::exists(cert_path) || !std::filesystem::exists(key_path)) {
        cert_manager_->generate_self_signed_certificate(cert_path, key_path);
    } else {
        cert_manager_->load_certificate(cert_path, key_path);
    }

    // Add routes to the HTTP server
    // server_->add_route("/info", boost::beast::http::verb::get, [this](auto&& req) {
    //     return rest_api_handler_->handle_info_request(req);
    // });

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

void NetworkManager::start_discovery() {
    if (discovery_manager_) {
        discovery_manager_->start(
            /* ������Դ��������еĶ˿ںţ���ʱ��Ĭ��ֵ */ config_.settings.port);
    }
}

void NetworkManager::stop_discovery() {
    if (discovery_manager_) {
        discovery_manager_->stop();
    }
}

std::vector<lansend::models::DeviceInfo> NetworkManager::get_discovered_devices() const {
    if (discovery_manager_) {
        return discovery_manager_->get_devices();
    }
    return {}; // Return an empty vector if discovery_manager_ is null
}

std::future<TransferResult> NetworkManager::send_file(const lansend::models::DeviceInfo& target,
                                                      const std::filesystem::path& filepath) {
    return std::async(std::launch::async, [this, target, filepath]() -> TransferResult {
        if (transfer_manager_) {
            return transfer_manager_->start_transfer(target, filepath);
        }
        TransferResult result;
        result.success = false;
        result.error_message = "Transfer manager not initialized.";
        result.transfer_id = 0;
        result.end_time = std::chrono::system_clock::now();
        return result;
    });
}

void NetworkManager::set_device_found_callback(
    std::function<void(const lansend::models::DeviceInfo&)> callback) {
    device_found_callback_ = callback;
    if (discovery_manager_) {
        discovery_manager_->set_device_found_callback(callback);
    }
}

void NetworkManager::set_transfer_progress_callback(
    std::function<void(const lansend::models::TransferProgress&)> callback) {
    transfer_progress_callback_ = callback;
}

void NetworkManager::set_transfer_complete_callback(
    std::function<void(const TransferResult&)> callback) {
    transfer_complete_callback_ = callback;
}