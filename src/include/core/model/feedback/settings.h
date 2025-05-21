#pragma once

#include "core/util/config.h"
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>

namespace lansend::core::feedback {

struct Settings {
    std::uint16_t port;
    std::string pin_code;
    bool auto_receive;
    std::string save_dir;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Settings, port, pin_code, auto_receive, save_dir);

    static Settings FromConfigSettings() {
        return Settings{
            .port = core::settings.port,
            .pin_code = core::settings.pin_code,
            .auto_receive = core::settings.auto_receive,
            .save_dir = core::settings.save_dir.string(),
        };
    }
};

} // namespace lansend::core::feedback