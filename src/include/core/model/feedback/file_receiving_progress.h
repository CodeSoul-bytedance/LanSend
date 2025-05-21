#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core::feedback {

struct FileReceivingProgress {
    std::string session_id;
    std::string filename;
    double progress;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileReceivingProgress, session_id, filename, progress);
};

} // namespace lansend::core::feedback