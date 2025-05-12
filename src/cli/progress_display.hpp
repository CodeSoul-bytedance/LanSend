#pragma once

#include "../models/transfer_progress.hpp"
#include <cstdint>
#include <string>

// struct TransferProgress {
//     uint64_t transfer_id;
//     uint64_t bytes_transferred;
//     uint64_t total_bytes;
//     double speed; // bytes per second
// };

class ProgressDisplay {
public:
    ProgressDisplay();
    ~ProgressDisplay();

    void update_progress(const lansend::models::TransferProgress& progress);
    void clear_progress();

private:
    void print_progress(const lansend::models::TransferProgress& progress);
};