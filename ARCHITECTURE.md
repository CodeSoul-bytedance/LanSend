1. 核心组件

1.1 核心网络管理 (NetworkManager)

位于 src/core/network_manager.hpp

职责：
- 管理所有网络相关组件
- 协调设备发现和文件传输
- 提供事件通知接口

主要接口：
class NetworkManager {
    void start(uint16_t port);
    void stop();
    void start_discovery();
    void stop_discovery();
    std::vector<DeviceInfo> get_discovered_devices() const;
    boost::asio::awaitable<TransferResult> send_file(
        const DeviceInfo& target,
        const std::filesystem::path& filepath
    );
    void set_device_found_callback(...);
    void set_transfer_progress_callback(...);
    void set_transfer_complete_callback(...);
};

1.2 传输管理 (TransferManager)

位于 src/transfer/transfer_manager.hpp

职责：
- 管理文件传输状态
- 处理传输进度
- 提供传输控制接口

主要接口：
class TransferManager {
    boost::asio::awaitable<TransferResult> start_transfer(...);
    void cancel_transfer(uint64_t transfer_id);
    std::optional<TransferState> get_transfer_state(...);
    std::vector<TransferState> get_active_transfers() const;
};

1.3 设备发现 (DiscoveryManager)

位于 src/discovery/discovery_manager.hpp

职责：
- 管理设备发现
- 处理设备状态更新
- 提供设备信息查询

主要接口：
class DiscoveryManager {
    void start(uint16_t port);
    void stop();
    void add_device(const DeviceInfo& device);
    void remove_device(const std::string& device_id);
    std::vector<DeviceInfo> get_devices() const;
    void set_device_found_callback(...);
    void set_device_lost_callback(...);
};

1.4 安全证书管理 (CertificateManager)

位于 src/security/certificate_manager.hpp

职责：
- 管理 TLS 证书
- 处理证书生成和加载
- 提供证书指纹

主要接口：
class CertificateManager {
    bool generate_self_signed_certificate(...);
    bool load_certificate(...);
    std::string get_certificate_fingerprint() const;
    boost::asio::ssl::context& get_ssl_context();
};

1.5 REST API 处理 (RestApiHandler)

位于 src/api/rest_api_handler.hpp

职责：
- 处理 HTTP 请求
- 实现 REST API 端点
- 提供事件流接口

文件传输相关端点:

这些端点负责协调文件的发送和接收，并支持中断后恢复传输。

class RestApiHandler {
    // --- 设备信息 ---
    boost::asio::awaitable<HttpResponse> handle_info_request(...); // 获取设备信息 (GET /info)

    // --- 文件传输发起与控制 ---
    // 发起传输请求 (POST /send)
    // 请求体 (JSON): { "filename": "...", "filesize": ..., "filehash": "...", "target_device_id": "..." }
    // 响应体 (JSON): { "transfer_id": ..., "status": "pending/accepted/..." }
    boost::asio::awaitable<HttpResponse> handle_send_request(...);

    // 接收方接受传输请求 (POST /accept)
    // 请求体 (JSON): { "transfer_id": ... }
    // 响应体 (JSON): { "status": "accepted" }
    boost::asio::awaitable<HttpResponse> handle_accept_request(...);

    // 接收方拒绝传输请求 (POST /reject)
    // 请求体 (JSON): { "transfer_id": ... }
    // 响应体 (JSON): { "status": "rejected" }
    boost::asio::awaitable<HttpResponse> handle_reject_request(...);

    // 完成传输（由接收方或发送方在数据传输完毕后调用）(POST /finish)
    // 请求体 (JSON): { "transfer_id": ... }
    // 响应体 (JSON): { "status": "finished" }
    boost::asio::awaitable<HttpResponse> handle_finish_transfer(...);

    // 取消传输 (POST /cancel)
    // 请求体 (JSON): { "transfer_id": ... }
    // 响应体 (JSON): { "status": "cancelled" }
    boost::asio::awaitable<HttpResponse> handle_cancel_transfer(...);

    // --- 断点续传核心接口 ---
    // 查询传输状态 (GET /status/{transfer_id})
    // 响应体 (JSON): { "transfer_id": ..., "filename": ..., "filesize": ..., "chunk_size": ..., "completed_chunks": [...], "total_chunks": ... }
    boost::asio::awaitable<HttpResponse> handle_get_transfer_status(..., uint64_t transfer_id);

    // 上传文件块（发送方推送数据）(POST /chunk/{transfer_id}/{chunk_index})
    // 请求体: 二进制文件块数据 (Content-Type: application/octet-stream)
    // 响应体: 空 或 状态确认 (e.g., 204 No Content, 200 OK)
    boost::asio::awaitable<HttpResponse> handle_upload_chunk(..., uint64_t transfer_id, uint64_t chunk_index);

    // 下载文件块（接收方拉取数据）(GET /chunk/{transfer_id}/{chunk_index})
    // 响应体: 二进制文件块数据 (Content-Type: application/octet-stream)
    boost::asio::awaitable<HttpResponse> handle_download_chunk(..., uint64_t transfer_id, uint64_t chunk_index);

    // --- 事件流 ---
    // 为客户端（如 Electron 前端）提供事件流 (GET /events)
    boost::asio::awaitable<HttpResponse> handle_events_stream(...);
};
注意: HttpResponse 是 boost::asio::awaitable<http::response<...>> 的简化表示。具体 Body 类型见 rest_api_handler.hpp。请求体和响应体格式仅为示例，具体实现可能有所不同。

1.6 HTTP 服务器 (HttpServer)

位于 src/api/http_server.hpp

职责：
- 管理 HTTP 服务器
- 处理路由
- 管理 SSL 连接

主要接口：
class HttpServer {
    void add_route(const std::string& path, http::verb method, RouteHandler handler);
    void start(uint16_t port);
    void stop();
};

1.7 文件哈希 (FileHasher)

位于 src/transfer/file_hasher.hpp

职责：
- 计算文件哈希
- 支持分块哈希计算

主要接口：
class FileHasher {
    std::string calculate_sha256_sync(...);
    bool start_chunked_sha256();
    bool update_chunk(...);
    std::string finalize_chunked_sha256();
};

1.8 命令行界面 (CLI)

位于 src/cli/ 目录下

职责：
- 提供命令行界面
- 处理用户输入
- 显示命令输出和进度

主要组件：

1. CLI 管理器 (CliManager)
class CliManager {
    void process_command(const std::string& command);
    void start_interactive_mode();
    void handle_list_devices();
    void handle_send_file(const std::string& device_id, const std::string& filepath);
    void handle_show_transfers();
    void handle_cancel_transfer(uint64_t transfer_id);
    void handle_show_help();
};

2. 参数解析器 (ArgumentParser)
class ArgumentParser {
    CliOptions parse();
    void show_help();
};

3. 终端界面 (Terminal)
class Terminal {
    void clear_screen();
    void set_cursor_position(int x, int y);
    std::string read_line();
    bool is_key_pressed();
};

4. 进度显示 (ProgressDisplay)
class ProgressDisplay {
    void update_progress(const TransferProgress& progress);
    void clear_progress();
};

支持的命令：
- list：列出可用设备
- send：发送文件到设备
- transfers：显示活动传输
- cancel：取消传输
- help：显示帮助信息

支持的选项：
- -p, --port：设置服务器端口
- -c, --config：设置配置文件路径
- -d, --daemon：以守护进程模式运行
- -l, --log-level：设置日志级别
- -h, --help：显示帮助信息

2. 工具类

2.1 配置管理 (Config)

位于 src/utils/
TODO
2.2 日志管理 (Logger)

位于 src/utils/
TODO
3. 构建系统

使用 CMake 构建系统，主要配置：
- C++23 标准
- 依赖：Boost, OpenSSL, spdlog

4. 与 Electron GUI 的集成

预留的集成接口：
1. 事件通知回调
  - 设备发现/丢失
  - 传输进度
  - 传输完成

2. 配置选项
  - GUI 端口和主机设置
  - 日志级别设置

3. 日志转发
  - 支持将日志转发到 GUI

5. 目录结构

src/
├── core/
│   └── network_manager.hpp
├── transfer/
│   ├── transfer_manager.hpp
│   └── file_hasher.hpp
├── discovery/
│   └── discovery_manager.hpp
├── security/
│   └── certificate_manager.hpp
├── api/
│   ├── rest_api_handler.hpp
│   └── http_server.hpp
├── cli/
│   ├── cli_manager.hpp
│   ├── argument_parser.hpp
│   ├── terminal.hpp
│   └── progress_display.hpp
├── utils/
│             
└── main.cpp
6. 架构图
[图片]
8. 文件传输流程 (含断点续传)

文件传输利用 TransferManager 和 TransferMetadata 进行状态管理和分块处理，并通过 RestApiHandler 提供的接口进行网络通信。

1. 初始化 (Initiation):
  - 发送方调用 /send API，提供文件名、大小、哈希等元数据和目标设备信息。
  - 服务器（发送方和接收方）创建 TransferMetadata 实例，记录传输信息（包括文件分块列表），分配唯一的 transfer_id。元数据会持久化存储（例如，在 .transfer 目录下生成 TOML 文件）。
  - 发送方通过某种机制（如事件流或设备发现广播）通知接收方有新的传输请求，包含 transfer_id。

2. 确认 (Confirmation):
  - 接收方收到通知后，可以通过 /info 或其他方式确认发送方信息。
  - 接收方调用 /accept 或 /reject API，表明是否接受传输。

3. 数据传输 (Data Transfer):
  - 基于拉取 (Pull-based，推荐用于局域网):
    - 接收方确认接受后，检查本地传输状态（通过加载 TransferMetadata 或调用发送方的 /status/{transfer_id} API）。
    - 确定需要下载的块索引 (chunk_index)。
    - 接收方循环调用发送方的 /chunk/{transfer_id}/{chunk_index} API (GET)，下载所需的文件块。
    - 每成功下载一个块，接收方更新本地 TransferMetadata 文件（标记块为完成）并保存。

4. 完成/取消 (Completion/Cancellation):
  - 完成: 所有块传输完毕后：
    - 接收方进行文件完整性校验（对比文件哈希）。
    - 接收方调用 /finish API 通知发送方传输成功完成。双方清理临时状态和文件。
  - 取消: 任何一方在传输过程中可以调用 /cancel API 中止传输。双方清理临时状态和文件。

5. 断点续传 (Resumption):
  - 如果传输在步骤 3 中断（例如，网络断开、应用关闭）。
  - 当应用重新启动或网络恢复时：
    - 发送方和接收方可以通过持久化的 TransferMetadata（基于 transfer_id 加载）恢复之前的传输状态。
    - 拉取模型: 接收方调用 /status/{transfer_id} 或检查本地元数据，找到第一个未完成的块，然后从步骤 3.a 继续执行 /chunk (GET) 请求。