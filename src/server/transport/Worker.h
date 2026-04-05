#pragma once

#include <thread>
#include <atomic>
#include <cstdint>
#include <string>
#include <map>

#include "SocketUtils.h"
#include "HttpsTransportServer.h"

namespace server::transport {

class Worker {
public:
    Worker(HttpsTransportServer* server);
    ~Worker();
    void Start();
    void Stop();

private:
    void Run();
    // handle a single accepted connection end-to-end
    void HandleConnection(std::intptr_t conn);
    // setup SSL for connection (if enabled); returns SSL* or nullptr
#ifdef HAVE_OPENSSL
    // forward-declare OpenSSL SSL type to avoid heavy include in header
    typedef struct ssl_st SSL;
    SSL* SetupSslForConnection(NativeSocket native, std::intptr_t conn);
#endif
    // process buffer and handle any complete HTTP requests; may update keepConnection
    void ProcessRequestsFromBuffer(std::intptr_t conn, std::string& buffer, NativeSocket native, bool& keepConnection);
    // cleanup SSL objects for a connection
    void CleanupSsl(std::intptr_t conn, void* ssl);
    HttpsTransportServer* server_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
};

} // namespace server::transport
