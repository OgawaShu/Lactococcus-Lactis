#include "Logger.h"

#include <iostream>

namespace server::runtime {

namespace {
std::string g_level = "info";
}

void Logger::Initialize(const std::string& level) {
    g_level = level;
    std::cout << "[INFO] logger initialized, level=" << g_level << std::endl;
}

void Logger::Info(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

void Logger::Error(const std::string& message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

}  // namespace server::runtime
