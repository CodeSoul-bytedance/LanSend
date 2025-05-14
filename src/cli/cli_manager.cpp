#include "cli_manager.hpp"
#include "../discovery/discovery_manager.hpp"
#include "../models/device_info.hpp"
#include "../models/transfer_progress.hpp"
#include "../transfer/transfer_manager.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

CliManager::CliManager(NetworkManager& network_manager)
    : terminal_(std::make_unique<Terminal>())
    , progress_display_(std::make_unique<ProgressDisplay>())
    , network_manager_(network_manager) {
    // 注册事件回调
    network_manager_.set_device_found_callback(
        [this](const lansend::models::DeviceInfo& device) { this->on_device_found(device); });
    network_manager_.set_transfer_progress_callback(
        [this](const lansend::models::TransferProgress& progress) {
            this->on_transfer_progress(progress);
        });
    network_manager_.set_transfer_complete_callback(
        [this](const TransferResult& result) { this->on_transfer_complete(result); });

    // // 解析命令行参数
    // auto options = argument_parser_->parse();
    // if (options.command.has_value()) {
    //     process_command(options.command.value());
    // }
}

CliManager::~CliManager() = default;

void CliManager::process_command(const std::string& command) {
    auto args = parse_command(command);
    execute_command(args);
}

void CliManager::start_interactive_mode() {
    terminal_->clear_screen();
    while (true) {
        terminal_->print_prompt();
        std::string command = terminal_->read_line();
        if (command == "exit" || command == "quit") {
            break;
        }
        process_command(command);
    }
}

void CliManager::handle_list_devices() {
    auto devices = network_manager_.get_discovered_devices();
    print_device_list(devices);
}

void CliManager::handle_send_file(const std::string& device_id, const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        terminal_->print_error("文件不存在: " + filepath);
        return;
    }
    try {
        auto devices = network_manager_.get_discovered_devices();
        std::optional<lansend::models::DeviceInfo> device_info;
        for (const auto& device : devices) {
            if (device.device_id == device_id) {
                device_info = device;
                break;
            }
        }
        if (!device_info.has_value()) {
            terminal_->print_error("no device ID is not found : " + device_id);
            return;
        }
        network_manager_.send_file(device_info.value(), filepath);
        terminal_->print_info("start to send: " + filepath + " to device ID: " + device_id);
    } catch (const std::exception& e) {
        terminal_->print_error("send file fail: " + std::string(e.what()));
    }
}

void CliManager::handle_show_transfers() {
    auto transfers = network_manager_.get_active_transfers();
    print_transfer_list(transfers);
}

void CliManager::handle_cancel_transfer(uint64_t transfer_id) {
    network_manager_.get_transfer_manager().cancel_transfer(transfer_id);
    terminal_->print_info("success cancel transfer，transfer ID: " + std::to_string(transfer_id));
}

void CliManager::handle_show_help() {
    // argument_parser_->show_help();
    print_help();
}

std::vector<std::string> CliManager::parse_command(const std::string& command) {
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    return args;
}

void CliManager::execute_command(const std::vector<std::string>& args) {
    if (args.empty()) {
        return;
    }
    const std::string& cmd = args[0];
    if (cmd == "list") {
        handle_list_devices();
        // terminal_->print_info("implement list function");
    } else if (cmd == "send") {
        if (args.size() >= 3) {
            handle_send_file(args[1], args[2]);
            // terminal_->print_info("implement send function "+args[1]+" "+args[2]);
        } else {
            terminal_->print_error("arguments is not enough!");
        }
    } else if (cmd == "transfers") {
        handle_show_transfers();
        // terminal_->print_info("implement transfers function");
    } else if (cmd == "cancel") {
        if (args.size() >= 2) {
            try {
                uint64_t transfer_id = std::stoull(args[1]);
                handle_cancel_transfer(transfer_id);
                // terminal_->print_info("implement cancel function "+args[1]);
            } catch (const std::exception& e) {
                terminal_->print_error("invalid transfer ID: " + args[1]);
            }
        } else {
            terminal_->print_error("arguments is not enough!");
        }

    } else if (cmd == "help") {
        handle_show_help();
    } else if (cmd == "clear") {
        terminal_->clear_screen();
    } else {
        terminal_->print_error("unknown command: " + cmd);
    }
}

void CliManager::print_device_list(const std::vector<lansend::models::DeviceInfo>& devices) {
    terminal_->print_info("devices list:");
    for (const auto& device : devices) {
        std::ostringstream oss;
        oss << "ID: " << device.device_id << " | 名称: " << device.alias
            << " | IP: " << device.ip_address;
        terminal_->print_info(oss.str());
    }
}

void CliManager::print_transfer_list(const std::vector<TransferState>& transfers) {
    terminal_->print_info("transfer list:");
    for (const auto& transfer : transfers) {
        std::ostringstream oss;
        std::string s;
        switch (transfer.status) {
        case TransferStatus::Pending:
            s = "PENDING";
            break;
        case TransferStatus::InProgress:
            s = "IN_PROGRESS";
            break;
        case TransferStatus::Completed:
            s = "COMPLETED";
            break;
        case TransferStatus::Failed:
            s = "FAILED";
            break;
        default:
            s = "Cancelled";
        }
        oss << "ID: " << transfer.id << " | state: " << s << std::endl;
        terminal_->print_info(oss.str());
    }
}

void CliManager::print_help() {
    terminal_->print_info("The available commands are as follows:");
    terminal_->print_info("  list - Lists the available devices");
    terminal_->print_info("  send <Device ID> <file path> - Send the file to the specified device");
    terminal_->print_info("  transfers - Displays a list of current transfers");
    terminal_->print_info("  cancel <transfer ID> - cancels the specified transfer");
    terminal_->print_info("  help - Displays this help message");
    terminal_->print_info("  exit/quit - Exits the program");
    terminal_->print_info("  clear - Clears the screen");
}

void CliManager::on_device_found(const lansend::models::DeviceInfo& device) {
    std::ostringstream oss;
    oss << "new device found: " << device.alias << " (" << device.ip_address << ")";
    terminal_->print_info(oss.str());
}

void CliManager::on_transfer_progress(const lansend::models::TransferProgress& progress) {
    progress_display_->update_progress(progress);
}

void CliManager::on_transfer_complete(const TransferResult& result) {
    if (result.success) {
        terminal_->print_info("Transfer completed: success，transfer ID: "
                              + std::to_string(result.transfer_id));
    } else {
        terminal_->print_error("Transfer completed: fail，transfer ID: "
                               + std::to_string(result.transfer_id)
                               + "，wrong information: " + result.error_message);
    }
    progress_display_->clear_progress();
}
