#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lansend::cli {

struct CliOptions {
    std::optional<uint16_t> port;
    std::optional<std::string> config_path;
    bool daemon_mode = false;
    std::optional<std::string> log_level;
    std::optional<std::string> command;
    std::vector<std::string> command_args;
};

class ArgumentParser {
public:
    ArgumentParser(int argc, char* argv[]);

    // 解析命令行参数
    CliOptions Parse();

    // 显示帮助信息
    void ShowHelp();

private:
    int argc_;
    char** argv_;
    int i; // 当前解析的参数索引

    // 参数解析
    void parseOptions(const std::string& arg, CliOptions& options);
    void parseCommand(CliOptions& options);

    // 参数验证
    bool validateOptions(const CliOptions& options);
};

} // namespace lansend::cli