#pragma once

#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace lansend::ipc::operation {

struct SendFiles {
    std::string device_id;
    std::vector<std::string> file_paths;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SendFiles, device_id, file_paths);
};

} // namespace lansend::ipc::operation