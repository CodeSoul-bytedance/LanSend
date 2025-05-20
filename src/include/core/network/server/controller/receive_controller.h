#pragma once

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
#include <unordered_map>
#include <unordered_set>

namespace lansend {

inline bool cancel_receive{false};

enum class ReceiveSessionStatus {
    kIdle,
    kReceiving,
};

using FileId = std::string;
using SessionId = std::string;

struct ReceiveFileContext {
    std::string file_name;                           // 文件名
    std::filesystem::path temp_file_path;            // 临时文件路径
    std::string file_token;                          // 文件令牌
    size_t file_size;                                // 文件总大小
    size_t chunk_size;                               // 块大小
    size_t total_chunks;                             // 总块数
    std::unordered_set<std::size_t> received_chunks; // 已接收块集合
    std::string file_checksum;                       // 整个文件的校验和
};

class ReceiveController {
public:
    using FileId = std::string;

    ReceiveController(HttpServer& server,
                      const std::filesystem::path& save_dir = path::kSystemDownloadDir);
    ~ReceiveController() = default;

    void SetSaveDirectory(const std::filesystem::path& save_dir);

    ReceiveSessionStatus session_status() const;
    std::string_view sender_ip() const;
    unsigned short sender_port() const;

    void NotifySenderLost();

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

    boost::asio::awaitable<std::optional<std::vector<FileDto>>> waitForUserConfirmation(
        std::string device_id, const std::vector<FileDto>& files, int timeout_seconds = 30);

    void installRoutes();
    void doCleanup(); // Clean up any unfinished temp files when cancelled or failed
    void checkSessionCompletion();

    HttpServer& server_;
    std::filesystem::path save_dir_;
    FileHasher file_hasher_;

    ReceiveSessionStatus session_status_{ReceiveSessionStatus::kIdle};
    std::string session_id_{};
    std::unordered_map<FileId, ReceiveFileContext> received_files_;
    std::size_t completed_count_{0};

    std::string sender_ip_{};
    unsigned short sender_port_{};
};

} // namespace lansend