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

    net::io_context ioc;
    ipc::IpcEventStream event_stream;
    // ipc::IpcService ipc_service(ioc, event_stream);
    ipc::IpcBackendService backend_service(ioc, event_stream);

    spdlog::info("lansend ipc backend started");

    // ipc_service.Start(); // communication with Electron
    backend_service.Start(); // core service

    ioc.run();

    SaveConfig();
}