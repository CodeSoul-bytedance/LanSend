#pragma once

#include "std_optional.h"
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace lansend::ipc::operation {

struct ConfirmReceive {
    bool accepted;
    std::optional<std::vector<std::string>> accepted_files;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConfirmReceive, accepted, accepted_files);
};

} // namespace lansend::ipc::operation