/*
    TODO: Too many errors in the code, remember fix this class to fit the new project design
*/

#pragma once

#include "../core/network/client/http_client_service.h"
#include "../core/model/device_info.h"
#include "../core/network/discovery/discovery_manager.h"
// #include "../models/transfer_progress.hpp"
#include "../core/util/logger.h"
#include "argument_parser.h"
#include "progress_display.h"
#include "terminal.h"
#include <memory>
#include <string>
#include <vector>

namespace lansend {
namespace api {
class HttpServer;
}
} // namespace lansend

class CliManager {
public:
    CliManager(lansend::HttpClientService& http_client_service,DiscoveryManager& discovery_manager);

    ~CliManager();

    // 命令处理
    void process_command(const std::string& command);
    void start_interactive_mode();

    // 命令实现
    void handle_list_devices();
    void handle_send_file(const std::string& device_id, const std::string& filepath);
    // void handle_show_transfers();
    // void handle_cancel_transfer(uint64_t transfer_id);
    void handle_show_help();

private:
    lansend::HttpClientService& http_client_service_;
    DiscoveryManager discovery_manager_;
    std::unique_ptr<Terminal> terminal_;
    std::unique_ptr<ProgressDisplay> progress_display_;
    std::unique_ptr<ArgumentParser> argument_parser;

    // 命令解析
    std::vector<std::string> parse_command(const std::string& command);
    void execute_command(const std::vector<std::string>& args);

    // 输出格式化 (使用明确的作用域解析 ::)
    void print_device_list(const std::vector<lansend::models::DeviceInfo>& devices);
    void print_transfer_list(const std::vector<TransferState>& transfers);
    void print_help();

    // 事件处理 (使用明确的作用域解析 ::)
    void on_device_found(const lansend::models::DeviceInfo& device);
    void on_transfer_progress(const lansend::models::TransferProgress& progress);
    void on_transfer_complete(const TransferResult& result);
};