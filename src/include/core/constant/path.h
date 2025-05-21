#pragma once

#include <filesystem>

#if defined(_WIN32) || defined(_WIN64)
#include <combaseapi.h>
#include <knownfolders.h>
#include <shlobj_core.h>
#endif

namespace lansend::core {
namespace path {

inline const std::filesystem::path kCertificateDir =
#if defined(_WIN32) || defined(_WIN64)
    std::filesystem::path(std::getenv("APPDATA")) / "CodeSoul" / "LanSend" / "certificates";
#elif defined(__APPLE__)
    std::filesystem::path(std::getenv("HOME")) / "Library" / "Application Support" / "CodeSoul"
    / "LanSend" / "certificates";
#else
    std::filesystem::path(std::getenv("HOME")) / ".local" / "share" / "CodeSoul" / "LanSend"
    / "certificates";
#endif

inline const std::filesystem::path kLogDir = std::filesystem::temp_directory_path() / "CodeSoul"
                                             / "LanSend" / "logs";

inline const std::filesystem::path kConfigDir =
#if defined(_WIN32) || defined(_WIN64)
    std::filesystem::path(std::getenv("APPDATA")) / "CodeSoul" / "LanSend";
#else
    std::filesystem::path(std::getenv("HOME")) / ".config" / "CodeSoul" / "LanSend";
#endif

inline const std::filesystem::path kSystemDownloadDir = []() -> std::filesystem::path {
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

} // namespace path
} // namespace lansend::core