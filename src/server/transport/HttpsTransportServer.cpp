#include "HttpsTransportServer.h"
#include "Acceptor.h"
#include "Worker.h"

#include <cstring>
#include <fstream>

#include "internal/parsing/HttpRequestParser.h"
#include "../runtime/Logger.h"
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <map>
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace server::transport {

namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;
#endif

NativeSocket ToNative(std::intptr_t value) {
    return static_cast<NativeSocket>(value);
}


std::intptr_t ToStorage(NativeSocket value) {
    return static_cast<std::intptr_t>(value);
}
}  // namespace

// Enqueue/Pop response implementations (outside anonymous namespace)
void HttpsTransportServer::EnqueueResponse(std::intptr_t connection, const std::string& response) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    connection_response_map_[connection] = response;
    connectionCv_.notify_all();
}

bool HttpsTransportServer::PopPendingResponse(std::intptr_t connection, std::string& out_response) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    auto it = connection_response_map_.find(connection);
    if (it == connection_response_map_.end()) return false;
    out_response = it->second;
    connection_response_map_.erase(it);
    return true;
}

bool HttpsTransportServer::HasPendingResponse(std::intptr_t connection) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return connection_response_map_.find(connection) != connection_response_map_.end();
}

bool HttpsTransportServer::WaitForPendingResponse(std::intptr_t connection, std::string& out_response) {
    std::unique_lock<std::mutex> lock(connectionMutex_);
    connectionCv_.wait(lock, [this, connection]() { return stopRequested_.load() || connection_response_map_.find(connection) != connection_response_map_.end(); });
    if (stopRequested_.load()) return false;
    auto it = connection_response_map_.find(connection);
    if (it == connection_response_map_.end()) return false;
    out_response = it->second;
    connection_response_map_.erase(it);
    return true;
}



std::intptr_t HttpsTransportServer::PopConnection() {
    std::unique_lock<std::mutex> lock(connectionMutex_);
    connectionCv_.wait(lock, [this]() { return !connectionQueue_.empty() || stopRequested_.load(); });
    if (connectionQueue_.empty()) return -1;
    auto c = connectionQueue_.front();
    connectionQueue_.pop();
    return c;
}

void HttpsTransportServer::EnqueueConnection(std::intptr_t conn) {
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        connectionQueue_.push(conn);
    }
    connectionCv_.notify_one();
}

void HttpsTransportServer::PushRequest(HttpTextRequest&& request) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push(std::move(request));
    }
    queueCv_.notify_one();
}

HttpsTransportServer::HttpsTransportServer(const config::ServerConfig& config) : config_(config) {}

HttpsTransportServer::~HttpsTransportServer() {
    Stop();
}

#ifdef HAVE_OPENSSL
static SSL_CTX* CreateSslContext(const std::string& certFile, const std::string& keyFile) {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) return nullptr;
    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    return ctx;
}
#endif

bool HttpsTransportServer::Start() {
    std::string error;
    if (!ValidateStartInputs(&error)) {
        runtime::Logger::Error(error);
        return false;
    }

    if (!InitializeSocketLayer()) {
        runtime::Logger::Error("failed to initialize socket layer");
        return false;
    }
    socketLayerInitialized_ = true;

#ifdef HAVE_OPENSSL
    // initialize openssl and create ctx
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ssl_ctx_ = CreateSslContext(config_.certificateFile, config_.privateKeyFile);
    if (!ssl_ctx_) {
        runtime::Logger::Error("failed to create SSL context");
        CleanupSocketLayer();
        socketLayerInitialized_ = false;
        return false;
    }
#endif

    const NativeSocket listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == kInvalidSocket) {
        runtime::Logger::Error("failed to create listener socket");
        CleanupSocketLayer();
        socketLayerInitialized_ = false;
        return false;
    }

    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.listenPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        runtime::Logger::Error("failed to bind listener socket");
        CloseSocket(ToStorage(listener));
        CleanupSocketLayer();
        socketLayerInitialized_ = false;
        return false;
    }

    if (listen(listener, 16) != 0) {
        runtime::Logger::Error("failed to listen on socket");
        CloseSocket(ToStorage(listener));
        CleanupSocketLayer();
        socketLayerInitialized_ = false;
        return false;
    }

    listenerSocket_ = ToStorage(listener);
    stopRequested_.store(false);
    running_.store(true);
    // start acceptor and workers via helper classes
    acceptor_ = std::make_unique<Acceptor>(this, config_);
    for (size_t i = 0; i < threadPoolSize_; ++i) {
        workers_.emplace_back(std::make_unique<Worker>(this));
    }
    for (auto &w : workers_) w->Start();
    acceptor_->Start(listenerSocket_);

    // (responder removed) business thread (ServerApplication) will enqueue responses

    // acceptor is responsible for accepting sockets; legacy AcceptLoop disabled

    runtime::Logger::Info("https listener started on 127.0.0.1:" + std::to_string(config_.listenPort));
    return true;
}

void HttpsTransportServer::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

    stopRequested_.store(true);
    if (listenerSocket_ != -1) {
        CloseSocket(listenerSocket_);
        listenerSocket_ = -1;
    }

    if (acceptor_) {
        acceptor_->Stop();
        acceptor_.reset();
    }
    // stop workers
    for (auto &w : workers_) {
        if (w) w->Stop();
    }
    workers_.clear();

    // responder removed - nothing to join

    queueCv_.notify_all();

    if (socketLayerInitialized_) {
        CleanupSocketLayer();
        socketLayerInitialized_ = false;
    }

#ifdef HAVE_OPENSSL
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
#ifdef HAVE_OPENSSL
    // free any remaining SSL objects
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        for (auto &kv : connection_ssl_map_) {
            if (kv.second) SSL_free(kv.second);
        }
        connection_ssl_map_.clear();
    }
#endif
#endif
}

bool HttpsTransportServer::IsRunning() const {
    return running_.load();
}

bool HttpsTransportServer::WaitAndPopHttpText(HttpTextRequest* request) {
    if (request == nullptr) {
        return false;
    }

    std::unique_lock<std::mutex> lock(queueMutex_);
    queueCv_.wait(lock, [this]() {
        return !queue_.empty() || stopRequested_.load();
    });

    if (queue_.empty()) {
        return false;
    }

    *request = std::move(queue_.front());
    queue_.pop();
    return true;
}

bool HttpsTransportServer::ValidateStartInputs(std::string* error) {
    if (config_.listenAddress != "127.0.0.1") {
        if (error != nullptr) {
            *error = "only 127.0.0.1 is allowed in current stage";
        }
        return false;
    }
    if (config_.listenPort == 0) {
        if (error != nullptr) {
            *error = "listenPort must be between 1 and 65535";
        }
        return false;
    }
    std::ifstream cert(config_.certificateFile);
    std::ifstream key(config_.privateKeyFile);
    if (!cert.good() || !key.good()) {
        // try to auto-generate self-signed certificate using openssl if available
        auto TryGenerate = [&]()->bool {
            // ensure parent dir exists
            try {
                std::filesystem::path certPath(config_.certificateFile);
                if (certPath.has_parent_path()) std::filesystem::create_directories(certPath.parent_path());
                std::filesystem::path keyPath(config_.privateKeyFile);
                if (keyPath.has_parent_path()) std::filesystem::create_directories(keyPath.parent_path());
            } catch (...) {}

            // build openssl command
            std::string cmd = "openssl req -x509 -newkey rsa:2048 -days 365 -nodes -keyout \"" + config_.privateKeyFile + "\" -out \"" + config_.certificateFile + "\" -subj \"/CN=localhost\" 2>nul";
            // Try execute; system returns implementation-defined code, treat 0 as success
            int rc = std::system(cmd.c_str());
            return rc == 0;
        };

        if (!TryGenerate()) {
            if (error != nullptr) {
                *error = "certificateFile/privateKeyFile not found and auto-generation failed";
            }
            return false;
        }
        // re-open
        cert.open(config_.certificateFile);
        key.open(config_.privateKeyFile);
        if (!cert.good() || !key.good()) {
            if (error != nullptr) {
                *error = "certificateFile/privateKeyFile not available after generation";
            }
            return false;
        }
    }
    return true;
}

void HttpsTransportServer::AcceptLoop() {
    while (!stopRequested_.load()) {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int clientAddressLen = sizeof(clientAddress);
#else
        socklen_t clientAddressLen = sizeof(clientAddress);
#endif

        NativeSocket client = accept(ToNative(listenerSocket_), reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLen);
        if (client == kInvalidSocket) {
            if (stopRequested_.load()) {
                break;
            }
            continue;
        }

        std::string rawPacket;
        char buffer[4096] = {};
        while (rawPacket.find("\r\n\r\n") == std::string::npos) {
            const int received = recv(client, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            rawPacket.append(buffer, received);
            if (rawPacket.size() > 65536) {
                break;
            }
        }

        // enqueue the accepted client socket for worker threads
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            connectionQueue_.push(ToStorage(client));
        }
        connectionCv_.notify_one();
    }
}

bool HttpsTransportServer::InitializeSocketLayer() {
#ifdef _WIN32
    WSADATA wsaData{};
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

void HttpsTransportServer::CleanupSocketLayer() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void HttpsTransportServer::CloseSocket(std::intptr_t socketValue) {
    const NativeSocket socket = ToNative(socketValue);
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

bool HttpsTransportServer::SendHttpResponse(std::intptr_t connection, const std::string& response) {
    if (connection == -1) return false;
    const NativeSocket native = ToNative(connection);
#ifdef HAVE_OPENSSL
    SSL* ssl = nullptr;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        auto it = connection_ssl_map_.find(connection);
        if (it != connection_ssl_map_.end()) ssl = it->second;
    }
    if (ssl) {
        int sent = SSL_write(ssl, response.data(), static_cast<int>(response.size()));
        return sent == static_cast<int>(response.size());
    }
#endif
    int sent = static_cast<int>(::send(native, response.data(), static_cast<int>(response.size()), 0));
    return sent == static_cast<int>(response.size());
}

#ifdef HAVE_OPENSSL
bool HttpsTransportServer::RegisterSslForConnection(std::intptr_t connection, SSL* ssl) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    connection_ssl_map_[connection] = ssl;
    return true;
}

void HttpsTransportServer::UnregisterSslForConnection(std::intptr_t connection) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    auto it = connection_ssl_map_.find(connection);
    if (it != connection_ssl_map_.end()) {
        SSL_free(it->second);
        connection_ssl_map_.erase(it);
    }
}
#endif

}  // namespace server::transport