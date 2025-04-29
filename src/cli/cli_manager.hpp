#pragma once

#include "../core/network_manager.hpp"
#include "../util/logger.hpp"
#include "progress_display.hpp"
#include "terminal.hpp"
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
    CliManager(NetworkManager& network_manager);
    ~CliManager();

    // 命令处理
    void process_command(const std::string& command);
    void start_interactive_mode();

    // 命令实现
    void handle_list_devices();
    void handle_send_file(const std::string& device_id, const std::string& filepath);
    void handle_show_transfers();
    void handle_cancel_transfer(uint64_t transfer_id);
    void handle_show_help();

private:
    NetworkManager& network_manager_;
    Logger& logger_;
    std::unique_ptr<Terminal> terminal_;
    std::unique_ptr<ProgressDisplay> progress_display_;

    // 命令解析
    std::vector<std::string> parse_command(const std::string& command);
    void execute_command(const std::vector<std::string>& args);

    // 输出格式化 (使用明确的作用域解析 ::)
    void print_device_list(const std::vector<DiscoveryManager::DeviceInfo>& devices);
    void print_transfer_list(const std::vector<TransferManager::TransferState>& transfers);
    void print_help();

    // 事件处理 (使用明确的作用域解析 ::)
    void on_device_found(const DiscoveryManager::DeviceInfo& device);
    void on_transfer_progress(const TransferManager::TransferProgress& progress);
    void on_transfer_complete(const TransferManager::TransferResult& result);
};