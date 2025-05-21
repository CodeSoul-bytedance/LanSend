#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/ip/udp.hpp>
#include <core/util/system.h>
#include <cstring>
#include <spdlog/spdlog.h>
#include <string>
#if defined(__APPLE__) || defined(__MACH__)
#include <errno.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/types.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace lansend::core {

namespace system {

std::string Hostname() {
    std::string hostname = boost::asio::ip::host_name();
    if (hostname.ends_with(".local")) {
        hostname = hostname.substr(0, hostname.size() - 6);
    } else if (hostname.ends_with(".localdomain")) {
        hostname = hostname.substr(0, hostname.size() - 13);
    } else if (hostname.ends_with(".domain")) {
        hostname = hostname.substr(0, hostname.size() - 7);
    }
    return hostname;
}

std::string PublicIpv4Address() {
    try {
        namespace net = boost::asio;
        net::io_context io_context;

        net::ip::udp::socket socket(io_context);

        socket.connect(
            boost::asio::ip::udp::endpoint(boost::asio::ip::make_address_v4("223.5.5.5"), 53));

        net::ip::udp::endpoint local_endpoint = socket.local_endpoint();
        std::string local_ip = local_endpoint.address().to_string();

        return local_ip;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get public IPv4 address: {}", e.what());
        return "127.0.0.1";
    }
}

std::string OperatingSystem() {
    std::string pretty_name{};
    constexpr auto architecture =
#if defined(__x86_64__) || defined(_M_X64)
        "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
        "x86";
#elif defined(__aarch64__)
        "aarch64";
#elif defined(__arm64__) || defined(_M_ARM64)
        "arm64";
#elif defined(__arm__) || defined(_M_ARM)
        "arm";
#else
        "Unknown";
#endif

#if defined(__APPLE__) || defined(__MACH__)
    char os_temp[20] = "";
    size_t os_temp_len = sizeof(os_temp);
    ushort major = 0, minor = 0, point = 0;

    int rslt = sysctlbyname("kern.osproductversion", os_temp, &os_temp_len, NULL, 0);
    if (rslt == 0) {
        sscanf(os_temp, "%hu.%hu.%hu", &major, &minor, &point);
        return std::format("macOS {}.{}.{} ({})", major, minor, point, architecture);
    } else {
        spdlog::error("sysctlbyname failed: {}", strerror(errno));
        return std::format("macOS ({})", architecture);
    }
#elif defined(__linux__)
    FILE* file = fopen("/etc/os-release", "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                pretty_name = line + 13;
                if (!pretty_name.empty() && pretty_name.back() == '\n') {
                    pretty_name.pop_back();
                }
                if (!pretty_name.empty() && pretty_name.back() == '"') {
                    pretty_name.pop_back();
                }
                break;
            }
        }
        fclose(file);
        return std::format("{} ({})", pretty_name, architecture);
    } else {
        return std::format("Linux ({})", architecture);
    }
#elif defined(_WIN32) || defined(_WIN64)
    OSVERSIONINFOEXW osInfo = {0};
    osInfo.dwOSVersionInfoSize = sizeof(osInfo);

    typedef LONG(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");

    if (RtlGetVersion != nullptr) {
        RtlGetVersion((PRTL_OSVERSIONINFOW) &osInfo);
    } else {
        return std::format("Windows ({})", architecture);
    }

    if (osInfo.dwMajorVersion == 10) {
        if (osInfo.dwMinorVersion == 0) {
            if (osInfo.dwBuildNumber >= 22000) {
                return std::format("Windows 11 ({})", architecture);
            }
            return std::format("Windows 10 ({})", architecture);
        }
    } else if (osInfo.dwMajorVersion == 6) {
        if (osInfo.dwMinorVersion == 3) {
            return std::format("Windows 8.1 ({})", architecture);
        } else if (osInfo.dwMinorVersion == 2) {
            return std::format("Windows 8 ({})", architecture);
        } else if (osInfo.dwMinorVersion == 1) {
            return std::format("Windows 7 ({})", architecture);
        } else if (osInfo.dwMinorVersion == 0) {
            return std::format("Windows Vista ({})", architecture);
        }
    } else if (osInfo.dwMajorVersion == 5) {
        if (osInfo.dwMinorVersion == 1) {
            return std::format("Windows XP ({})", architecture);
        } else if (osInfo.dwMinorVersion == 0) {
            return std::format("Windows 2000 ({})", architecture);
        }
    }

    return std::format("Windows {}.{} ({})",
                       osInfo.dwMajorVersion,
                       osInfo.dwMinorVersion,
                       architecture);
#elif defined(__FreeBSD__)
    return std::format("FreeBSD ({})", architecture);
#else
#warning "This OS is not supported yet. Please report this issue to the developer."
    return std::format("Unknown OS ({})", architecture);
#endif
}

} // namespace system

} // namespace lansend::core