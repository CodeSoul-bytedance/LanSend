#include "core/network_manager.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    lansend::Logger logger(
#ifdef LANSEND_DEBUG
        lansend::Logger::Level::debug,
#else
        lansend::Logger::Level::info,
#endif
        (std::filesystem::temp_directory_path() / "CodeSoul" / "LanSend" / "logs" / "evento.log")
            .string());
    lansend::init_config();
    spdlog::info("Hello, welcome to LanSend!");
    lansend::save_config();
    return 0;
}