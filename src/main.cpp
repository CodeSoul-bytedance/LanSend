#include "utils/config.hpp"
#include "utils/logger.hpp"
#include <boost/asio.hpp>
#include <constants/path.hpp>

int main(int argc, char* argv[]) {
    lansend::Logger logger(
#ifdef LANSEND_DEBUG
        lansend::Logger::Level::debug,
#else
        lansend::Logger::Level::info,
#endif
        (lansend::path::kLogDir / "lansend.log").string());
    lansend::init_config();
    spdlog::info("Hello, welcome to LanSend!");
    lansend::save_config();
    return 0;
}