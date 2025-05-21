#include <boost/asio/io_context.hpp>
#include <core/constant/path.h>
#include <core/security/open_ssl_provider.h>
#include <core/util/config.h>
#include <core/util/logger.h>
#include <ipc/ipc_backend_service.h>
#include <ipc/ipc_event_stream.h>
#include <ipc/ipc_service.h>

using namespace lansend;
using namespace lansend::core;
namespace net = boost::asio;

int main(int argc, char* argv[]) {
    Logger logger(
#ifdef LANSEND_DEBUG
        Logger::Level::debug,
#else
        Logger::Level::info,
#endif
        path::kLogDir.string()); // Convert std::filesystem::path to std::string
    InitConfig();

    OpenSSLProvider::InitOpenSSL();

    // 解析命令行参数，获取命名管道名称
    std::string stdin_pipe_name;
    std::string stdout_pipe_name;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--stdin-pipe-name" && i + 1 < argc) {
            stdin_pipe_name = argv[++i];
        } else if (arg == "--stdout-pipe-name" && i + 1 < argc) {
            stdout_pipe_name = argv[++i];
        }
    }

    net::io_context ioc;
    ipc::IpcEventStream event_stream;
    ipc::IpcService ipc_service(ioc, event_stream, stdin_pipe_name, stdout_pipe_name);
    ipc::IpcBackendService backend_service(ioc, event_stream);

    spdlog::info("lansend ipc backend started");

    ipc_service.start();     // communication with Electron
    backend_service.Start(); // core service

    ioc.run();

    SaveConfig();
}