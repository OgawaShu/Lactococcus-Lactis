#pragma once

#include <atomic>
#include <string>

#include "../config/ServerConfig.h"
#include "../database/DatabaseStore.h"
#include "../transport/HttpsTransportServer.h"

namespace server::bootstrap {

class ServerApplication {
public:
    explicit ServerApplication(const config::ServerConfig& config);
    bool Start();
    void Stop(const std::string& reason);
    void Wait();

private:
    bool InitializeDatabase(std::string* errorMessage);
    void ProcessHttpText(const transport::HttpTextRequest& request);

    config::ServerConfig config_;
    database::DatabaseStore databaseStore_;
    transport::HttpsTransportServer transportServer_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
};

}  // namespace server::bootstrap}  // namespace server::bootstrap