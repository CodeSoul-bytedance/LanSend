#pragma once

#include "operation_type.h"
#include <nlohmann/json.hpp>
#include <optional>

namespace lansend::ipc {

struct Operation {
    OperationType type;
    nlohmann::json data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Operation, type, data);

    template<typename T>
    std::optional<T> getData() const {
        try {
            return data.get<T>();
        } catch (...) {
            return std::nullopt;
        }
    }
};

} // namespace lansend::ipc