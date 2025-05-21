#pragma once

#include <string>

namespace lansend::core {

namespace system {

std::string Hostname();
std::string PublicIpv4Address();
std::string OperatingSystem(); // etc: Windows 11 (x86_64)

} // namespace system

} // namespace lansend::core