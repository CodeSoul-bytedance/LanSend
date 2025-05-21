#pragma once

#include "feedback/device_connect_result.h"
#include "feedback/feedback_type.h"
#include "feedback/file_receiving_completed.h"
#include "feedback/file_receiving_progress.h"
#include "feedback/file_sending_completed.h"
#include "feedback/file_sending_progress.h"
#include "feedback/found_device.h"
#include "feedback/lost_device.h"
#include "feedback/receive_session_end.h"
#include "feedback/recipient_accepted.h"
#include "feedback/recipient_declined.h"
#include "feedback/request_receive_files.h"
#include "feedback/send_session_end.h"
#include "feedback/settings.h"
#include <nlohmann/json.hpp>

namespace lansend::core {
struct Feedback {
    FeedbackType type;
    nlohmann::json data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Feedback, type, data);
};

using FeedbackCallback = std::function<void(Feedback&&)>;

} // namespace lansend::core