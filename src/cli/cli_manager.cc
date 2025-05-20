#include "cli_manager.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "../core/util/logger.h"

namespace fs = std::filesystem;

CliManager::CliManager(lansend::HttpClientService& http_client_service,DiscoveryManager& discovery_manager)
    : http_client_service_(http_client_service),
      discovery_manager_(discovery_manager),
      terminal_(std::make_unique<Terminal>()),
      progress_display_(std::make_unique<ProgressDisplay>()),
      argument_parser(std::make_unique<ArgumentParser>()) {
    
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
    try {
        auto devices = discovery_manager_.GetDevices();
        print_device_list(devices);
    } catch (const std::exception& e) {
        terminal_->print_error("Failed to list devices: " + std::string(e.what()));
    }
}

void CliManager::handle_send_file(const std::string& alias, const std::vector<std::string>& filepaths) {
    std::vector<std::filesystem::path> valid_file_paths;
    for (const auto& filepath : filepaths) {
        if (!fs::exists(filepath)) {
            terminal_->print_error("file not exist: " + filepath);
            continue;
        }
        valid_file_paths.emplace_back(filepath);
    }

    if (valid_file_paths.empty()) {
        terminal_->print_error("No valid files to send.");
        return;
    }
    try {
        auto devices = discovery_manager_.GetDevices();
        auto it = devices.end();
        for (auto iter = devices.begin(); iter != devices.end(); ++iter) {
            if (iter->alias == alias) {
                it = iter;
                break;
            }
        }
        if (it == devices.end()) {
            terminal_->print_error("device not found alias: " + alias);
            return;
        }
        http_client_service_.SendFiles(it->ip_address, it->port, file_paths);
        std::ostringstream oss;
        oss << "start sending files to device ID: " << alias << "\n";
        for (const auto& filepath : filepaths) {
            oss << " - " << filepath << "\n";
        }
        terminal_->print_info(oss.str());
    } catch (const std::exception& e) {
        terminal_->print_error("send file fail: " + std::string(e.what()));
    }
}

// void CliManager::handle_show_transfers() {
//     try {
//         auto transfers = http_client_service_.get_active_transfers();
//         print_transfer_list(transfers);
//     } catch (const std::exception& e) {
//         terminal_->print_error("Failed to show transfers: " + std::string(e.what()));
//     }
// }

// void CliManager::handle_cancel_transfer(uint64_t transfer_id) {
//     try {
//         http_client_service_.cancel_transfer(transfer_id);
//         terminal_->print_info("success cancel transfer，transfer ID: " + std::to_string(transfer_id));
//     } catch (const std::exception& e) {
//         terminal_->print_error("fail cancel transfer: " + std::string(e.what()));
//     }
// }

void CliManager::handle_show_help() {
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
    } else if (cmd == "send") {
        if (args.size() >= 3) {
            const std::string alias = args[1];
            std::vector<std::string> filepaths(args.begin() + 2, args.end());
            handle_send_file(alias, filepaths);
        } else {
            terminal_->print_error("args too less, correct format: send <device ID> <file path>");
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
    terminal_->print_info("device list:");
    for (const auto& device : devices) {
        std::ostringstream oss;
        oss << "ID: " << device.device_id << " | alias: " << device.alias
            << " | IP: " << device.ip_address;
        terminal_->print_info(oss.str());
    }
}

void CliManager::print_help() {
    terminal_->print_info("Available commands are as follows:");
    terminal_->print_info("  list - List available devices");
    terminal_->print_info("  send <device ID> <file path> - Send a file to the specified device");
    terminal_->print_info("  help - Show this help information");
    terminal_->print_info("  exit/quit - Exit the program");
    terminal_->print_info("  clear - Clear the screen");

}
