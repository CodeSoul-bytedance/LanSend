#pragma once

#include "model/transfer_progress.h"
#include <cstdint>
#include <string>

namespace lansend::cli {

class ProgressDisplay {
public:
    ProgressDisplay();
    ~ProgressDisplay();

    void UpdateProgress(const TransferProgress& progress);
    void ClearProgress();

private:
    void printProgress(const TransferProgress& progress);
};

} // namespace lansend::cli