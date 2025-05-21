#pragma once

#include "feedback_type.h"
#include <nlohmann/json.hpp>

namespace lansend::core {

struct Feedback {
    FeedbackType type;
    nlohmann::json data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Feedback, type, data);
};

} // namespace lansend::core