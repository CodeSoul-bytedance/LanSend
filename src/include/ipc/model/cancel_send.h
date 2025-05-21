#pragma once

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace lansend::ipc::operation {

struct CancelSend {
    std::string session_id; // 传输的session_id

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CancelSend, session_id);
};

} // namespace lansend::ipc::operation