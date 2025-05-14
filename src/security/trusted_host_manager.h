#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

class TrustedHostManager {
public:
    TrustedHostManager(const std::filesystem::path& store_path);
    ~TrustedHostManager();

    void load_trusted();

    void save_trusted();

    void trust_host(const std::string& hostname, const std::string& fingerprint);

    bool is_trusted(const std::string& hostname, const std::string& fingerprint);

    std::string get_fingerprint(const std::string& hostname) const;

    void remove_host(const std::string& hostname);

    const std::unordered_map<std::string, std::string>& trusted_hosts() const;

private:
    std::filesystem::path trust_store_path_;
    std::unordered_map<std::string, std::string> trusted_hosts_;
};