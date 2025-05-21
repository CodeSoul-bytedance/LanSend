#pragma once

#include "std_optional.h"
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace lansend::ipc {

struct ConfirmReceiveOperation {
    bool accepted;
    std::optional<std::vector<std::string>> accepted_files;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConfirmReceiveOperation, accepted, accepted_files);
};

} // namespace lansend::ipc