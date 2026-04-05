#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "../config/ServerConfig.h"
#include "TransportTypes.h"
#include <map>
#include <mutex>
#include <vector>
#include <memory>

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

namespace server::transport {

class HttpsTransportServer {
public:
    explicit HttpsTransportServer(const config::ServerConfig& config);
    ~HttpsTransportServer();
    bool Start();
    void Stop();
    bool IsRunning() const;
    bool WaitAndPopHttpText(HttpTextRequest* request);
    // Enqueue a response for a request previously received on `connection`.
    // Worker thread that owns the connection will send the response and close the connection.
    void EnqueueResponse(std::intptr_t connection, const std::string& response);
    // Pop a pending response for a connection (worker uses this to retrieve response submitted by business thread)
    bool PopPendingResponse(std::intptr_t connection, std::string& out_response);
    // Wait for a pending response for a connection. Blocks until a response is available or server stop requested.
    bool WaitForPendingResponse(std::intptr_t connection, std::string& out_response);
    // Check without popping if a pending response exists
    bool HasPendingResponse(std::intptr_t connection);

    // synchronous send used by worker threads
    bool SendHttpResponse(std::intptr_t connection, const std::string& response);


private:
    bool ValidateStartInputs(std::string* error);
    void AcceptLoop();
    static bool InitializeSocketLayer();
    static void CleanupSocketLayer();
    static void CloseSocket(std::intptr_t socketValue);

    config::ServerConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::intptr_t listenerSocket_ = -1;
    bool socketLayerInitialized_ = false;
    std::thread acceptThread_;

    // connection handling thread pool for accepted client sockets
    std::vector<std::thread> workerThreads_;
    std::mutex connectionMutex_;
    std::condition_variable connectionCv_;
    std::queue<std::intptr_t> connectionQueue_;
    size_t threadPoolSize_ = 4;
    // TLS helpers (only available when OpenSSL is enabled)
#ifdef HAVE_OPENSSL
    SSL_CTX* GetSslCtx() { return ssl_ctx_; }
    bool RegisterSslForConnection(std::intptr_t connection, SSL* ssl);
    void UnregisterSslForConnection(std::intptr_t connection);
#endif
    // connection queue helpers
    std::intptr_t PopConnection();
    void EnqueueConnection(std::intptr_t conn);
    void PushRequest(HttpTextRequest&& request);
    // make these public-ish for helper classes (Acceptor/Worker) via friendship
    friend class Acceptor;
    friend class Worker;
    std::map<std::intptr_t, std::string> connection_response_map_;
    // expose internal maps for worker helpers
    std::mutex& GetConnectionMutex() { return connectionMutex_; }
#ifdef HAVE_OPENSSL
    std::map<std::intptr_t, SSL*>& GetSslMap() { return connection_ssl_map_; }
#endif
    // acceptor and workers
    std::unique_ptr<class Acceptor> acceptor_;
    std::vector<std::unique_ptr<class Worker>> workers_;

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::queue<HttpTextRequest> queue_;

#ifdef HAVE_OPENSSL
    SSL_CTX* ssl_ctx_ = nullptr;
    // map from connection id to SSL* for sending responses from main thread
    std::map<std::intptr_t, SSL*> connection_ssl_map_;
#endif

    // Send a raw HTTP response text back to the connection that originated a request.
    // Returns true on success.
    // (implemented in the .cpp)
};

}  // namespace server::transport