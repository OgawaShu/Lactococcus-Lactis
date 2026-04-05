#pragma once

#include <string>

namespace server::runtime {

class Logger {
public:
    static void Initialize(const std::string& level);
    static void Info(const std::string& message);
    static void Error(const std::string& message);
};

}  // namespace server::runtime
