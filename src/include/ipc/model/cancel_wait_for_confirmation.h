#pragma once

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace lansend::ipc::operation {

struct CancelWaitForConfirmation {
    std::string device_id; // id of the receiver device

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CancelWaitForConfirmation, device_id);
};

} // namespace lansend::ipc::operation