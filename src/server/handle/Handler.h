// Replaced by PascalCase file naming
#pragma once

#include <string>
#include <map>
#include <utility>

#include "../database/DatabaseManager.h"
#include "../auth/SessionManager.h"

class Handler {
public:
    Handler(DatabaseManager &db, SessionManager &sm);

    std::pair<int, std::string> dispatch(const std::string &method,
                                         const std::string &path,
                                         const std::map<std::string, std::string> &headers,
                                         const std::string &body);

private:
    std::pair<int, std::string> handleRegister(const std::string &method,
                                                const std::string &path,
                                                const std::map<std::string, std::string> &headers,
                                                const std::string &body);

    DatabaseManager &db;
    SessionManager &session_manager;
};
