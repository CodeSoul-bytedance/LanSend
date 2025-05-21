#pragma once

#include <cstddef>

namespace lansend::core {

namespace transfer {

constexpr size_t kDefaultChunkSize = 1 * 1024 * 1024; // 1 MB
constexpr size_t kMaxChunkSize = 32 * 1024 * 1024;    // 32 MB

} // namespace transfer

} // namespace lansend::core