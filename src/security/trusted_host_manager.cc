#include "trusted_host_manager.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

TrustedHostManager::TrustedHostManager(const fs::path& storePath) {
    trust_store_path_ = storePath / "trusted_fingerprints.json";
    load_trusted();
}

TrustedHostManager::~TrustedHostManager() {}

void TrustedHostManager::load_trusted() {
    if (!fs::exists(trust_store_path_)) {
        spdlog::debug("No trusted fingerprints file found, will create on first connection");
        return;
    }

    try {
        std::ifstream file(trust_store_path_);
        json j = json::parse(file);

        for (const auto& [host, fingerprint] : j.items()) {
            trusted_hosts_[host] = fingerprint.get<std::string>();
        }

        spdlog::debug("Loaded {} trusted host fingerprints", trusted_hosts_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to load trusted fingerprints: {}", e.what());
    }
}

void TrustedHostManager::save_trusted() {
    try {
        json j;
        for (const auto& [host, fingerprint] : trusted_hosts_) {
            j[host] = fingerprint;
        }

        std::ofstream file(trust_store_path_);
        file << j.dump(2);
        file.close();

        spdlog::debug("Saved {} trusted host fingerprints", trusted_hosts_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to save trusted fingerprints: {}", e.what());
    }
}

void TrustedHostManager::trust_host(const std::string& hostname, const std::string& fingerprint) {
    trusted_hosts_[hostname] = fingerprint;
    save_trusted();
}

bool TrustedHostManager::is_trusted(const std::string& hostname, const std::string& fingerprint) {
    return trusted_hosts_.contains(hostname) && trusted_hosts_.at(hostname) == fingerprint;
}

std::string TrustedHostManager::get_fingerprint(const std::string& hostname) const {
    if (trusted_hosts_.contains(hostname)) {
        return trusted_hosts_.at(hostname);
    }
    return "";
}

void TrustedHostManager::remove_host(const std::string& hostname) {
    trusted_hosts_.erase(hostname);
    save_trusted();
}

const std::unordered_map<std::string, std::string>& TrustedHostManager::trusted_hosts() const {
    return trusted_hosts_;
}