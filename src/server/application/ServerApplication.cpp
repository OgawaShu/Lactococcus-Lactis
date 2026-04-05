#include "ServerApplication.h"

#include <csignal>

#include "../../common/protocol/password/ApplyPassword.h"
#include "../../common/protocol/user/RegisterUser.h"
#include "../../common/http_util/HttpToStruct.h"
#include "../runtime/Logger.h"

namespace server::bootstrap {

namespace {
ServerApplication* g_app = nullptr;

void OnSignal(int signal) {
    if (g_app != nullptr) {
        g_app->Stop("signal=" + std::to_string(signal));
    }
}
}  // namespace

ServerApplication::ServerApplication(const config::ServerConfig& config)
    : config_(config),
      transportServer_(config_) {}

bool ServerApplication::Start() {
    runtime::Logger::Initialize(config_.logLevel);

    std::string dbError;
    if (!InitializeDatabase(&dbError)) {
        runtime::Logger::Error("database init failed: " + dbError);
        return false;
    }

    if (!transportServer_.Start()) {
        runtime::Logger::Error("transport startup failed");
        return false;
    }

    g_app = this;
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    running_.store(true);
    runtime::Logger::Info("server started");
    return true;
}

void ServerApplication::Stop(const std::string& reason) {
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true)) {
        return;
    }

    runtime::Logger::Info("server stopping, reason=" + reason);
    transportServer_.Stop();
    running_.store(false);
}

void ServerApplication::Wait() {
    while (running_.load()) {
        transport::HttpTextRequest request;
        if (!transportServer_.WaitAndPopHttpText(&request)) {
            if (!running_.load()) {
                break;
            }
            continue;
        }

        ProcessHttpText(request);
    }
}

bool ServerApplication::InitializeDatabase(std::string* errorMessage) {
    return databaseStore_.Initialize(config_.databaseFile, "sql/init.sql", errorMessage);
}

void ServerApplication::ProcessHttpText(const transport::HttpTextRequest& request) {
    runtime::Logger::Info("main thread received http text: method=" + request.method + ", path=" + request.path +
                          ", bodySize=" + std::to_string(request.body.size()));

    std::string dbError;
    if (request.path == "/register-user") {
        protocol::user::RegisterUserRequest registerRequest;
        if (!http_util::ToProtocolStruct(request, &registerRequest)) {
            runtime::Logger::Error("register parse failed");
            // respond 400
            std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            transportServer_.EnqueueResponse(request.connection, resp);
            return;
        }

        if (!databaseStore_.SaveRegisterUser(registerRequest, &dbError)) {
            runtime::Logger::Error("register persistence failed: " + dbError);
            std::string body = "register failed: " + dbError;
            std::string resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            transportServer_.EnqueueResponse(request.connection, resp);
            return;
        }

        runtime::Logger::Info("register persistence success");
        std::string body = "ok";
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        transportServer_.EnqueueResponse(request.connection, resp);
        return;
    }

    if (request.path == "/apply-password") {
        protocol::password::ApplyPasswordRequest applyRequest;
        if (!http_util::ToProtocolStruct(request, &applyRequest)) {
            runtime::Logger::Error("apply-password parse failed");
            std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            transportServer_.EnqueueResponse(request.connection, resp);
            return;
        }

        if (!databaseStore_.SaveApplyPassword(applyRequest, &dbError)) {
            runtime::Logger::Error("apply-password persistence failed: " + dbError);
            std::string body = "apply failed: " + dbError;
            std::string resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            transportServer_.EnqueueResponse(request.connection, resp);
            return;
        }

        runtime::Logger::Info("apply-password persistence success");
        std::string body = "ok";
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        transportServer_.EnqueueResponse(request.connection, resp);
        return;
    }

    runtime::Logger::Info("TODO: unsupported path, no persistence action");
    // no automatic test responder here; business logic should enqueue responses explicitly
}

}  // namespace server::bootstrap