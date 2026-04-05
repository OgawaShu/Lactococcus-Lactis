#include "Worker.h"
#include "HttpsTransportServer.h"
#include "SocketUtils.h"
#include "internal/parsing/HttpRequestParser.h"
#include "../runtime/Logger.h"
#include <sstream>
#include <cctype>

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#endif

namespace server::transport {

Worker::Worker(HttpsTransportServer* server) : server_(server) {}
Worker::~Worker() { Stop(); }

void Worker::Start() {
    stop_.store(false);
    thread_ = std::thread([this]() { Run(); });
}

void Worker::Stop() {
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
}

void Worker::Run() {
    while (!stop_.load()) {
        std::intptr_t conn = server_->PopConnection();
        if (conn == -1) continue;
        HandleConnection(conn);
    }
}

void Worker::HandleConnection(std::intptr_t conn) {
    NativeSocket native = ToNative(conn);
    std::string buffer;
    char tmp[4096];
    void* ssl = nullptr;
#ifdef HAVE_OPENSSL
    ssl = static_cast<void*>(SetupSslForConnection(native, conn));
    if (ssl == nullptr) return;
#endif

    bool keepConnection = true;
    while (!stop_.load() && keepConnection) {
#ifdef HAVE_OPENSSL
        int received = SSL_read(ssl, tmp, sizeof(tmp));
#else
        int received = recv(native, tmp, sizeof(tmp), 0);
#endif
        if (received <= 0) break;
        buffer.append(tmp, received);
        ProcessRequestsFromBuffer(conn, buffer, native, keepConnection);
    }

    CleanupSsl(conn, ssl);
    CloseSocket(conn);
}

#ifdef HAVE_OPENSSL
SSL* Worker::SetupSslForConnection(NativeSocket native, std::intptr_t conn) {
    SSL* ssl = SSL_new(server_->GetSslCtx());
    if (!ssl) return nullptr;
    BIO* bio = BIO_new_socket(native, BIO_NOCLOSE);
    SSL_set_bio(ssl, bio, bio);
    if (SSL_accept(ssl) <= 0) {
        SSL_free(ssl);
        return nullptr;
    }
    server_->RegisterSslForConnection(conn, ssl);
    return ssl;
}
#endif

void Worker::ProcessRequestsFromBuffer(std::intptr_t conn, std::string& buffer, NativeSocket native, bool& keepConnection) {
    while (true) {
        const auto headerEndPos = buffer.find("\r\n\r\n");
        if (headerEndPos == std::string::npos) break;
        const size_t headerLen = headerEndPos + 4;
        int contentLen = 0;
        std::string headerPart = buffer.substr(0, headerEndPos);
        std::istringstream hs(headerPart);
        std::string line;
        if (!std::getline(hs, line)) break;
        while (std::getline(hs, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string val = pos + 1 < line.size() ? line.substr(pos + 1) : std::string();
                while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(val.begin());
                if (key == "Content-Length" || key == "content-length") {
                    try { contentLen = std::stoi(val); } catch (...) { contentLen = 0; }
                }
            }
        }

        const size_t totalNeeded = headerLen + static_cast<size_t>(contentLen);
        if (buffer.size() < totalNeeded) break;

        std::string rawRequest = buffer.substr(0, totalNeeded);
        buffer.erase(0, totalNeeded);

        HttpTextRequest request;
        if (parsing::ParseHttpText(rawRequest, &request)) {
            request.connection = conn;
            // decide keep-alive based on request headers and version
            auto ShouldKeepAlive = [](const HttpTextRequest& r)->bool {
                for (const auto& kv : r.headers) {
                    std::string key = kv.first;
                    std::string val = kv.second;
                    for (auto &c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (key == "connection") {
                        for (auto &c : val) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (val.find("close") != std::string::npos) return false;
                        if (val.find("keep-alive") != std::string::npos) return true;
                    }
                }
                if (r.version == "HTTP/1.1") return true;
                return false;
            };

            bool keep_alive = ShouldKeepAlive(request);
            runtime::Logger::Info("Worker: parsed request, enqueuing to main queue");
            server_->PushRequest(std::move(request));

            std::string pending;
            if (server_->WaitForPendingResponse(conn, pending)) {
                server_->SendHttpResponse(conn, pending);
            } else {
                keepConnection = false;
                break;
            }

            if (!keep_alive) {
                keepConnection = false;
                break;
            }
        } else {
            runtime::Logger::Error("failed to parse packet as http text");
        }
    }
}

void Worker::CleanupSsl(std::intptr_t conn, void* ssl) {
#ifdef HAVE_OPENSSL
    if (ssl) {
        std::lock_guard<std::mutex> lock(server_->GetConnectionMutex());
        auto it = server_->GetSslMap().find(conn);
        if (it != server_->GetSslMap().end()) {
            SSL_free(it->second);
            server_->GetSslMap().erase(it);
        }
    }
#endif
    (void)ssl;
}

} // namespace server::transport
