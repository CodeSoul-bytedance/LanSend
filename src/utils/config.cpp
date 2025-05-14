#include "config.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

#if defined(_WIN32) || defined(_WIN64)
// clang-format off
#include <shlobj.h>
#include <KnownFolders.h>
// clang-format on
#endif

namespace lansend {
namespace details {

const std::filesystem::path config_dir =
#if defined(_WIN32) || defined(_WIN64)
    std::filesystem::path(std::getenv("APPDATA")) / "CodeSoul" / "LanSend";
#else
    std::filesystem::path(std::getenv("HOME")) / ".config" / "CodeSoul" / "LanSend";
#endif

const std::filesystem::path default_save_dir = [] {
#if defined(_WIN32) || defined(_WIN64)
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path))) {
        auto saveDir = std::filesystem::path(path);
        CoTaskMemFree(path);
        return saveDir;
    } else {
        return std::filesystem::path(std::getenv("USERPROFILE")) / "Downloads";
    }
#else
    return std::filesystem::path(std::getenv("HOME")) / "Downloads";
#endif
}();

} // namespace details

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
        settings.saveDir = setting["save-dir"].value_or(details::default_save_dir.string());
    } else {
        settings.saveDir = details::default_save_dir;
    }
}

void init_config() {
    if (!std::filesystem::exists(details::config_dir)) {
        spdlog::info("Config directory does not exist, creating...");
        std::filesystem::create_directories(details::config_dir);
    }
    auto path = details::config_dir / "config.toml";
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
    auto path = details::config_dir / "config.toml";
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