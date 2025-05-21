// #include <cli/cli_manager.h>
// #include <filesystem>
// #include <sstream>

// namespace fs = std::filesystem;

// using namespace lansend::core;

// namespace lansend::cli {

// CliManager::CliManager(HttpClientService& http_client_service, DiscoveryManager& discovery_manager)
//     : http_client_service_(http_client_service)
//     , discovery_manager_(discovery_manager)
//     , terminal_(std::make_unique<Terminal>())
//     , progress_display_(std::make_unique<ProgressDisplay>())
//     , argument_parser(std::make_unique<ArgumentParser>(0, nullptr)) {}

// CliManager::~CliManager() = default;

void CliManager::process_command(const std::string& command) {
    auto args = parse_command(command);
    execute_command(args);
}
void CliManager::start_interactive_mode() {
    terminal_->ClearScreen();
    while (true) {
        terminal_->PrintPrompt();
        std::string command = terminal_->ReadLine();
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
        terminal_->PrintError("Failed to list devices: " + std::string(e.what()));
    }
}

void CliManager::handle_send_file(const std::string& alias,
                                  const std::vector<std::filesystem::path>& filepaths) {
    std::vector<std::filesystem::path> valid_file_paths;
    for (const auto& filepath : filepaths) {
        if (!fs::exists(filepath)) {
            terminal_->PrintError("file not exist: " + filepath.string());
            continue;
        }
        valid_file_paths.emplace_back(filepath);
    }

    if (valid_file_paths.empty()) {
        terminal_->PrintError("No valid files to send.");
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
            terminal_->PrintError("device not found alias: " + alias);
            return;
        }
        http_client_service_.SendFiles(it->ip_address, it->port, filepaths);
        std::ostringstream oss;
        oss << "start sending files to device ID: " << alias << "\n";
        for (const auto& filepath : filepaths) {
            oss << " - " << filepath << "\n";
        }
        terminal_->PrintInfo(oss.str());
    } catch (const std::exception& e) {
        terminal_->PrintError("send file fail: " + std::string(e.what()));
    }
}

// void CliManager::handle_show_transfers() {
//     try {
//         auto transfers = http_client_service_.get_active_transfers();
//         print_transfer_list(transfers);
//         auto transfers = http_client_service_.get_active_transfers();
//         print_transfer_list(transfers);
//     } catch (const std::exception& e) {
//         terminal_->print_error("Failed to show transfers: " + std::string(e.what()));
//         terminal_->print_error("Failed to show transfers: " + std::string(e.what()));
//     }
// }

// void CliManager::handle_cancel_transfer(uint64_t transfer_id) {
//     try {
//         http_client_service_.cancel_transfer(transfer_id);
//         terminal_->print_info("success cancel transfer，transfer ID: " + std::to_string(transfer_id));
//     } catch (const std::exception& e) {
//         terminal_->print_error("fail cancel transfer: " + std::string(e.what()));
//     try {
//         auto devices = discovery_manager_.GetDevices();
//         auto it = devices.end();
//         for (auto iter = devices.begin(); iter != devices.end(); ++iter) {
//             if (iter->alias == alias) {
//                 it = iter;
//                 break;
//             }
//         }
//         if (it == devices.end()) {
//             terminal_->PrintError("device not found alias: " + alias);
//             return;
//         }
//         http_client_service_.SendFiles(it->ip_address, it->port, filepaths);
//         std::ostringstream oss;
//         oss << "start sending files to device ID: " << alias << "\n";
//         for (const auto& filepath : filepaths) {
//             oss << " - " << filepath << "\n";
//         }
//         terminal_->PrintInfo(oss.str());
//     } catch (const std::exception& e) {
//         terminal_->PrintError("send file fail: " + std::string(e.what()));
//     }
// }

// // void CliManager::handle_show_transfers() {
// //     try {
// //         auto transfers = http_client_service_.get_active_transfers();
// //         print_transfer_list(transfers);
// //     } catch (const std::exception& e) {
// //         terminal_->print_error("Failed to show transfers: " + std::string(e.what()));
// //     }
// // }

// // void CliManager::handle_cancel_transfer(uint64_t transfer_id) {
// //     try {
// //         http_client_service_.cancel_transfer(transfer_id);
// //         terminal_->print_info("success cancel transfer，transfer ID: " + std::to_string(transfer_id));
// //     } catch (const std::exception& e) {
// //         terminal_->print_error("fail cancel transfer: " + std::string(e.what()));
// //     }
// // }

// void CliManager::handle_show_help() {
//     print_help();
// }

// std::vector<std::string> CliManager::parse_command(const std::string& command) {
//     std::vector<std::string> args;
//     std::istringstream iss(command);
//     std::string arg;
//     while (iss >> arg) {
//         args.push_back(arg);
//     }
//     return args;
// }

// void CliManager::execute_command(const std::vector<std::string>& args) {
//     if (args.empty()) {
//         return;
//     }
//     const std::string& cmd = args[0];
//     if (cmd == "list") {
//         handle_list_devices();
//     } else if (cmd == "send") {
//         if (args.size() >= 3) {
//             const std::string alias = args[1];
//             std::vector<std::string> filepaths_string(args.begin() + 2, args.end());
//             std::vector<fs::path> filepaths;
//             for (const auto& filepath : filepaths_string) {
//                 filepaths.emplace_back(filepath);
//             }
//             handle_send_file(alias, filepaths);
//         } else {
//             terminal_->PrintError("args too less, correct format: send <device ID> <file path>");
//         }
//     } else if (cmd == "help") {
//         handle_show_help();
//     } else if (cmd == "clear") {
//         terminal_->ClearScreen();
//     } else {
//         terminal_->PrintError("unknown command: " + cmd);
//     }
// }

void CliManager::print_device_list(const std::vector<DeviceInfo>& devices) {
    terminal_->PrintInfo("device list:");
    for (const auto& device : devices) {
        std::ostringstream oss;
        oss << "ID: " << device.device_id << " | IP: " << device.ip_address;
        terminal_->PrintInfo(oss.str());
    }
}

// void CliManager::print_help() {
//     terminal_->PrintInfo("Available commands are as follows:");
//     terminal_->PrintInfo("  list - List available devices");
//     terminal_->PrintInfo("  send <device ID> <file path> - Send a file to the specified device");
//     terminal_->PrintInfo("  help - Show this help information");
//     terminal_->PrintInfo("  exit/quit - Exit the program");
//     terminal_->PrintInfo("  clear - Clear the screen");
// }

} // namespace lansend::cli