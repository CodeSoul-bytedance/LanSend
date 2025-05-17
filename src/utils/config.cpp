#include "config.hpp"
#include <constants/path.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

#if defined(_WIN32) || defined(_WIN64)
// clang-format off
#include <shlobj.h>
#include <KnownFolders.h>
// clang-format on
#endif

namespace lansend {

static void load_setting() {
    if (!config.contains("setting")) {
        config.insert("setting", toml::table{});
    }
    auto& setting = config["setting"].ref<toml::table>();

    if (setting.contains("alias")) {
        settings.alias = setting["alias"].value_or("");
    } else {
        settings.alias = "LanSend";
    }
    if (setting.contains("port")) {
        settings.port = setting["port"].value_or(56789);
    } else {
        settings.port = 56789;
    }
    if (setting.contains("auth-code")) {
        settings.authCode = setting["auth-code"].value_or("");
    } else {
        settings.authCode = "";
    }
    if (setting.contains("auto-save")) {
        settings.autoSave = setting["auto-save"].value_or(false);
    } else {
        settings.autoSave = false;
    }
    if (setting.contains("save-dir")) {
        settings.saveDir = setting["save-dir"].value_or(path::kSystemDownloadDir.string());
    } else {
        settings.saveDir = path::kSystemDownloadDir;
    }
}

void init_config() {
    if (!std::filesystem::exists(path::kConfigDir)) {
        spdlog::info("Config directory does not exist, creating...");
        std::filesystem::create_directories(path::kConfigDir);
    }
    auto path = path::kConfigDir / "config.toml";
    if (!std::filesystem::exists(path)) {
        std::ofstream ofs(path);
        spdlog::info("Config file does not exist, creating...");
    }
    try {
        config = toml::parse_file(path.u8string());
    } catch (const toml::parse_error& err) {
        spdlog::error("\"{}\" could not be opened for parsing.", path.string());
        config = toml::parse("");
    }

    load_setting();
}

void save_config() {
    auto path = path::kConfigDir / "config.toml";
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        spdlog::error("Failed to open \"{}\" for saving config.", path.string());
        return;
    }
    config.insert_or_assign("setting",
                            toml::table{
                                {"alias", settings.alias},
                                {"port", settings.port},
                                {"auth-code", settings.authCode},
                                {"auto-save", settings.autoSave},
                                {"save-dir", settings.saveDir.string()},
                            });
    ofs << config;
}

} // namespace lansend