#pragma once

#include <thread>
#include <atomic>
#include <cstdint>
#include "SocketUtils.h"
#include "HttpsTransportServer.h"

namespace server::transport {

class Acceptor {
public:
    Acceptor(HttpsTransportServer* server, const config::ServerConfig& config);
    ~Acceptor();
    void Start(std::intptr_t listenerSocket);
    void Stop();

private:
    void Run();

    HttpsTransportServer* server_;
    const config::ServerConfig& config_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::intptr_t listenerSocket_ = -1;
};

} // namespace server::transport
