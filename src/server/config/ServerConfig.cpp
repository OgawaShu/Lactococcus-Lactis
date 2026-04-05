#include "ServerConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

namespace server::config {

namespace {
std::string Trim(std::string value) {
    auto isNotSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), isNotSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(), value.end());
    return value;
}

std::unordered_map<std::string, std::string> ParseKeyValueFile(const std::string& filePath) {
    std::unordered_map<std::string, std::string> values;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return values;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = Trim(trimmed.substr(0, separator));
        const std::string value = Trim(trimmed.substr(separator + 1));
        if (!key.empty()) {
            values[key] = value;
        }
    }

    return values;
}

std::uint16_t ParsePort(const std::string& text) {
    try {
        return static_cast<std::uint16_t>(std::stoul(text));
    } catch (...) {
        return 0;
    }
}

}  // namespace

bool ServerConfigLoader::LoadFromFile(const std::string& filePath, ServerConfig* config, std::string* errorMessage) {
    if (config == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "config output pointer is null";
        }
        return false;
    }

    const auto values = ParseKeyValueFile(filePath);
    if (values.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "failed to load config file: " + filePath;
        }
        return false;
    }

    config->listenAddress = values.count("listenAddress") > 0 ? values.at("listenAddress") : "";
    config->listenPort = values.count("listenPort") > 0 ? ParsePort(values.at("listenPort")) : 0;
    config->logLevel = values.count("logLevel") > 0 ? values.at("logLevel") : "";
    config->certificateFile = values.count("certificateFile") > 0 ? values.at("certificateFile") : "";
    config->privateKeyFile = values.count("privateKeyFile") > 0 ? values.at("privateKeyFile") : "";
    config->databaseFile = values.count("databaseFile") > 0 ? values.at("databaseFile") : "";

    return Validate(*config, errorMessage);
}

bool ServerConfigLoader::Validate(const ServerConfig& config, std::string* errorMessage) {
    if (config.listenAddress.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "listenAddress is required";
        }
        return false;
    }

    if (config.listenPort == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "listenPort must be between 1 and 65535";
        }
        return false;
    }

    if (config.logLevel.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "logLevel is required";
        }
        return false;
    }

    if (config.certificateFile.empty() || config.privateKeyFile.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "certificateFile and privateKeyFile are required";
        }
        return false;
    }

    if (config.databaseFile.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "databaseFile is required";
        }
        return false;
    }

    return true;
}

}  // namespace server::config