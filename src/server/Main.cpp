#include <string>

#include "application/ServerApplication.h"
#include "config/ServerConfig.h"
#include "runtime/Logger.h"

int main() {
    server::config::ServerConfig config;
    std::string error;
    if (!server::config::ServerConfigLoader::LoadFromFile("src/server/server.config", &config, &error)) {
        server::runtime::Logger::Error("config load failed: " + error);
        return 1;
    }

    server::bootstrap::ServerApplication app(config);
    if (!app.Start()) {
        return 1;
    }

    app.Wait();
    return 0;
}