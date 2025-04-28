#include "core/network_manager.hpp"
#include "util/config/config.hpp"
#include "util/logger/logger.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    try {
        // 初始化配置
        if (!Config::get_instance().load(argc, argv)) {
            std::cerr << "Failed to load configuration" << std::endl;
            return 1;
        }

        // 初始化日志
        Logger::get_instance().init(Logger::Level::Info, "lansend.log");

        // 创建IO上下文
        boost::asio::io_context ioc;

        // 创建网络管理器
        NetworkManager network_manager(ioc);

        // 启动服务
        network_manager.start(Config::get_instance().get_https_port());

        // 运行IO上下文
        ioc.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}