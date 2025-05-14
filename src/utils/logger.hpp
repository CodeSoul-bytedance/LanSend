#pragma once

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/details/thread_pool.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace lansend {
class Logger {
    using LoggerType = spdlog::async_logger;

public:
    using Level = spdlog::level::level_enum;
    Logger(Level level,
           const std::string& filepath,
           std::size_t q_max_items = 8192,
           std::size_t thread_count = 1) {
        spdlog::file_event_handlers handlers;
        handlers.after_open = [](const spdlog::filename_t& filename, std::FILE* fstream) {
            if (fstream) {
                auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                std::fprintf(fstream, "[Log Start: %s | %s]\n", filename.c_str(), std::ctime(&now));
            }
        };
        handlers.before_close = [](const spdlog::filename_t& filename, std::FILE* fstream) {
            if (fstream) {
                auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                std::fprintf(fstream, "[Log End: %s | %s]\n\n", filename.c_str(), std::ctime(&now));
            }
        };
        auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(filepath,
                                                                             0,
                                                                             0,
                                                                             false,
                                                                             0,
                                                                             handlers);
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        if constexpr (std::is_same_v<LoggerType, spdlog::async_logger>) {
            thread_pool_ = std::make_shared<spdlog::details::thread_pool>(q_max_items, thread_count);
            logger_ = std::make_shared<LoggerType>("lansend",
                                                   spdlog::sinks_init_list{file_sink, stdout_sink},
                                                   thread_pool_);
        } else {
            logger_ = std::make_shared<LoggerType>("lansend",
                                                   spdlog::sinks_init_list{file_sink, stdout_sink});
        }

#ifdef LANSEND_RELEASE
        stdout_sink->set_level(Level::off);
#endif
        logger_->set_level(level);
        logger_->set_error_handler(
            [&](const std::string& msg) { logger_->error("*** LOGGER ERROR ***: {}", msg); });
        // spdlog::register_logger(_asyncLogger);

        /* more about pattern:
         * https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
         */
        stdout_sink->set_pattern("\033[36m[%Y-%m-%d %H:%M:%S.%e] \033[92m[%n] \033[0m%^[%l]%$ %v");
        spdlog::set_default_logger(logger_);
    }
    auto& logger() { return logger_; }
    [[nodiscard]] Level logLevel() const { return logger_->level(); }
    void set_log_level(Level level) { logger_->set_level(level); }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    ~Logger() { spdlog::shutdown(); }

private:
    std::shared_ptr<LoggerType> logger_;
    std::shared_ptr<spdlog::details::thread_pool> thread_pool_;
};
} // namespace lansend
