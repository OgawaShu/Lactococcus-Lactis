#include "Acceptor.h"
#include "HttpsTransportServer.h"
#include "SocketUtils.h"
#include "internal/parsing/HttpRequestParser.h"
#include "../runtime/Logger.h"

namespace server::transport {

Acceptor::Acceptor(HttpsTransportServer* server, const config::ServerConfig& config) : server_(server), config_(config) {}
Acceptor::~Acceptor() { Stop(); }

void Acceptor::Start(std::intptr_t listenerSocket) {
    listenerSocket_ = listenerSocket;
    stop_.store(false);
    thread_ = std::thread([this]() { Run(); });
}

void Acceptor::Stop() {
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
}

void Acceptor::Run() {
    while (!stop_.load()) {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int clientAddressLen = sizeof(clientAddress);
#else
        socklen_t clientAddressLen = sizeof(clientAddress);
#endif
        NativeSocket client = accept(ToNative(listenerSocket_), reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLen);
        if (client == kInvalidSocket) {
            if (stop_.load()) break;
            continue;
        }

        // enqueue the accepted client socket for worker threads
        server_->EnqueueConnection(ToStorage(client)); // Enqueue the accepted client socket
    }
}

} // namespace server::transport
