#pragma once

#include "operation_type.h"
#include <nlohmann/json.hpp>

namespace lansend::ipc {

struct Operation {
    OperationType type;
    nlohmann::json data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Operation, type, data);
};

} // namespace lansend::ipc