#include <core/constant/path.h>
#include <core/util/config.h>
#include <fstream>
#include <spdlog/spdlog.h>

#if defined(_WIN32) || defined(_WIN64)
// clang-format off
#include <shlobj.h>
#include <KnownFolders.h>
// clang-format on
#endif

namespace lansend::core {

static void LoadSetting() {
    if (!config.contains("setting")) {
        config.insert("setting", toml::table{});
    }
    auto& setting = config["setting"].ref<toml::table>();

    if (setting.contains("port")) {
        settings.port = setting["port"].value_or(56789);
    } else {
        settings.port = 56789;
    }
    if (setting.contains("pin-code")) {
        settings.pin_code = setting["pin-code"].value_or("");
    } else {
        settings.pin_code = "";
    }
    if (setting.contains("auto-receive")) {
        settings.auto_receive = setting["auto-receive"].value_or(false);
    } else {
        settings.auto_receive = false;
    }
    if (setting.contains("save-dir")) {
        settings.save_dir = setting["save-dir"].value_or(path::kSystemDownloadDir.string());
    } else {
        settings.save_dir = path::kSystemDownloadDir;
    }
}

void InitConfig() {
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

    LoadSetting();
}

void SaveConfig() {
    auto path = path::kConfigDir / "config.toml";
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        spdlog::error("Failed to open \"{}\" for saving config.", path.string());
        return;
    }
    config.insert_or_assign("setting",
                            toml::table{
                                {"port", settings.port},
                                {"pin-code", settings.pin_code},
                                {"auto-receive", settings.auto_receive},
                                {"save-dir", settings.save_dir.string()},
                            });
    ofs << config;
}

} // namespace lansend::core