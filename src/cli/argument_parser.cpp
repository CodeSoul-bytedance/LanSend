#include "argument_parser.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>

ArgumentParser::ArgumentParser(int argc, char* argv[])
    : argc_(argc)
    , argv_(argv)
    , i(1) {}

CliOptions ArgumentParser::parse() {
    CliOptions options;

    // 解析选项
    while (i < argc_) {
        std::string arg = argv_[i];

        if (arg[0] != '-') {
            // 遇到非选项参数，开始解析命令
            parse_command(options);
            break;
        }

        parse_option(arg, options);
        i++;
    }

    if (!validate_options(options)) {
        show_help();
        std::exit(1);
    }

    return options;
}

void ArgumentParser::parse_option(const std::string& arg, CliOptions& options) {
    if (arg == "-p" || arg == "--port") {
        if (++i >= argc_) {
            throw std::runtime_error("Missing port number");
        }
        options.port = std::stoi(argv_[i]);
    } else if (arg == "-c" || arg == "--config") {
        if (++i >= argc_) {
            throw std::runtime_error("Missing config path");
        }
        options.config_path = argv_[i];
    } else if (arg == "-d" || arg == "--daemon") {
        options.daemon_mode = true;
    } else if (arg == "-l" || arg == "--log-level") {
        if (++i >= argc_) {
            throw std::runtime_error("Missing log level");
        }
        options.log_level = argv_[i];
    } else if (arg == "-h" || arg == "--help") {
        show_help();
        std::exit(0);
    } else {
        throw std::runtime_error("Unknown option: " + arg);
    }
}

void ArgumentParser::parse_command(CliOptions& options) {
    if (i >= argc_) {
        return;
    }

    options.command = argv_[i++];

    // 收集命令参数
    while (i < argc_) {
        options.command_args.push_back(argv_[i++]);
    }
}

bool ArgumentParser::validate_options(const CliOptions& options) {
    if (options.port && (*options.port < 1024 || *options.port > 65535)) {
        std::cerr << "Error: Port must be between 1024 and 65535" << std::endl;
        return false;
    }

    if (options.log_level) {
        std::string level = *options.log_level;
        std::transform(level.begin(), level.end(), level.begin(), ::tolower);
        if (level != "debug" && level != "info" && level != "warning" && level != "error") {
            std::cerr << "Error: Invalid log level" << std::endl;
            return false;
        }
    }

    return true;
}

void ArgumentParser::show_help() {
    std::cout << "Usage: lansend [options] [command] [args...]\n\n"
              << "Options:\n"
              << "  -p, --port PORT      Set server port (default: 8080)\n"
              << "  -c, --config PATH    Set config file path\n"
              << "  -d, --daemon         Run in daemon mode\n"
              << "  -l, --log-level LVL  Set log level (debug|info|warning|error)\n"
              << "  -h, --help           Show this help message\n\n"
              << "Commands:\n"
              << "  list                 List available devices\n"
              << "  send FILE DEVICE     Send file to device\n"
              << "  transfers            Show active transfers\n"
              << "  cancel ID            Cancel transfer by ID\n"
              << "  help                 Show help message\n";
}