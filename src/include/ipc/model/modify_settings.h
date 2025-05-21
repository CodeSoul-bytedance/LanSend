#pragma once

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace lansend::ipc::operation {

struct ModifySettings {
    std::string key;
    nlohmann::json value;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModifySettings, key, value);
};

} // namespace lansend::ipc::operation