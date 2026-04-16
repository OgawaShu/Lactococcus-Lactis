// Simple in-memory session record for authentication flows
#pragma once

#include <string>
#include <chrono>

struct Session {
    enum class State { Pending, Completed, Expired };

    // Public fields for simple data holder
    std::string token;
    std::string challenge;
    std::string public_key;
    long user_id = 0;
    State state = State::Pending;
    std::chrono::system_clock::time_point expires;
};

// Helper factory and utilities (lightweight C API)
Session createSessionWithTtl(std::chrono::seconds ttl);
bool sessionIsExpired(const Session &s);
void sessionMarkCompleted(Session &s, std::string public_key, long user_id);
