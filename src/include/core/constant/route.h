#pragma once

#include <string_view>

namespace lansend::core {

class ApiRoute {
public:
    static constexpr std::string_view kPing = "/ping";
    static constexpr std::string_view kConnect = "/connect";
    static constexpr std::string_view kRequestSend = "/request-send";
    static constexpr std::string_view kSendChunk = "/send-chunk";
    static constexpr std::string_view kVerifyIntegrity = "/verify-integrity";
    static constexpr std::string_view kCancelSend = "/cancel-send";
};

} // namespace lansend::core