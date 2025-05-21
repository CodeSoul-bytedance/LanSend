#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace lansend::core::feedback {

struct RecipientAccepted {
    std::string device_id;
    std::vector<std::string> accepted_files;
    std::string session_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RecipientAccepted, device_id, accepted_files, session_id);
};

} // namespace lansend::core::feedback