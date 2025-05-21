#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core::feedback {

struct RecipientDeclined {
    std::string device_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RecipientDeclined, device_id);
};

} // namespace lansend::core::feedback