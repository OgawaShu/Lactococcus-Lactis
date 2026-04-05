#pragma once

#include <cstdint>
#include <string>

namespace server::config {

struct ServerConfig {
    std::string listenAddress;
    std::uint16_t listenPort = 0;
    std::string logLevel;
    std::string certificateFile;
    std::string privateKeyFile;
    std::string databaseFile;
    // number of seconds to keep an idle keep-alive connection open
    int keepAliveTimeoutSeconds = 30;
    // maximum number of requests allowed per connection before server closes it
    int maxRequestsPerConnection = 100;
    // number of worker threads for transport server connection handling
    int threadPoolSize = 4;
};

class ServerConfigLoader {
public:
    static bool LoadFromFile(const std::string& filePath, ServerConfig* config, std::string* errorMessage);
    static bool Validate(const ServerConfig& config, std::string* errorMessage);
};

}  // namespace server::config