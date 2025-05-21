/*
    config.h
    This header provides functionality for managing application configuration
    using TOML files. It includes utilities for reading and writing general
    configuration values as well as specific application settings.

    Example usage:

    General configuration:
    - Read a value from the general config:
        T value = lansend::config["key"].value_or(default_value);
    - Write a value to the general config:
        lansend::config["key"].insert_or_assign("key", value);

    Application settings:
    - Read a setting:
        std::string alias = lansend::settings.alias;
        std::uint16_t port = lansend::settings.port;
        std::string pin_code = lansend::settings.pin_code;
        bool auto_receive = lansend::settings.auto_receive;
        std::filesystem::path saveDir = lansend::settings.saveDir;
    - Write a setting:
        lansend::settings.alias = "new_alias";
        lansend::settings.port = 9999;
        lansend::settings.pin_code = "new_pin_code";
        lansend::settings.auto_receive = true;
        lansend::settings.save_dir = "/path/to/save";

    Initialization and saving:
    - Initialize the configuration (loads from file or creates default):
        lansend::initConfig();
    - Save the current configuration to file:
        lansend::saveConfig();
*/

#pragma once

#include <filesystem>
#include <string>
#include <toml++/toml.h>

namespace lansend::core {

inline toml::table config;

struct Settings {
    std::string device_id;
    std::string alias;              // Display name
    std::uint16_t port;             // Server port
    std::string pin_code;           // Pin Code for other devices to connect
    bool auto_receive;              // Whether to automatically receive files from other devices
    std::filesystem::path save_dir; // Directory to save files from other devices
    std::filesystem::path metadataStoragePath;
    uint64_t chunkSize;
    bool https = true; // Whether to use HTTPS instead of HTTP
};

inline Settings settings;

void InitConfig();

void SaveConfig();

} // namespace lansend::core