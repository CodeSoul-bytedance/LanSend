#pragma once

#include "core/model/feedback.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <core/constant/path.h>
#include <core/model.h>
#include <core/network/server/http_server.h>
#include <core/security/file_hasher.h>
#include <filesystem>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core {

enum class ReceiveSessionStatus {
    kIdle,
    kWorking,
};

using FileId = std::string;
using SessionId = std::string;

class ReceiveController {
public:
    using FileId = std::string;
    using WaitConditionFunc = std::function<std::optional<std::vector<std::string>>()>;
    using CancelConditionFunc = std::function<bool()>;

    ReceiveController(HttpServer& server,
                      const std::filesystem::path& save_dir = path::kSystemDownloadDir,
                      FeedbackCallback callback = nullptr);
    ~ReceiveController() = default;

    void SetSaveDirectory(const std::filesystem::path& save_dir);

    ReceiveSessionStatus session_status() const;
    std::string_view sender_ip() const;
    unsigned short sender_port() const;

    void NotifySenderLost(); // Called by Controller's HttpServer when sender is lost

    void SetFeedbackCallback(FeedbackCallback callback);
    void SetWaitConditionFunc(WaitConditionFunc func);
    void SetCancelConditionFunc(CancelConditionFunc func);

    // 重置接收控制器状态为空闲，取消当前的接收会话
    void resetToIdle();

private:
    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
    onRequestSend(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> onSendChunk(
        const boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
    onVerifyIntegrity(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
    onCancelSend(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
    onCancelWait(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::asio::awaitable<std::optional<std::vector<FileDto>>> waitForUserConfirmation(
        std::string device_id, const std::vector<FileDto>& files, int timeout_seconds = 30);

    void installRoutes();
    void doCleanup(); // Clean up any unfinished temp files when cancelled or failed
    void checkSessionCompletion();

    HttpServer& server_;
    std::filesystem::path save_dir_;
    FeedbackCallback callback_;
    WaitConditionFunc wait_condition_;
    CancelConditionFunc cancel_condition_;

    ReceiveSessionStatus session_status_{ReceiveSessionStatus::kIdle};
    std::string session_id_{};
    std::unordered_map<FileId, ReceiveFileContext> received_files_;
    std::size_t completed_file_count_{0};

    std::string sender_ip_{};
    unsigned short sender_port_{};

    void feedback(Feedback&& feedback) {
        if (callback_) {
            callback_(std::move(feedback));
        }
    }
};

} // namespace lansend::core