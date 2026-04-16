#pragma once

#include "Session.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <string>

class SessionManager {
public:
    SessionManager() = default;

    // create a new session with ttl
    std::shared_ptr<Session> create(std::chrono::seconds ttl);

    // find session by token, returns nullptr if not found or expired
    std::shared_ptr<Session> find(const std::string &token);

    // mark session completed
    bool complete(const std::string &token, const std::string &public_key, long user_id);

    // remove expired sessions
    void sweep();

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};
