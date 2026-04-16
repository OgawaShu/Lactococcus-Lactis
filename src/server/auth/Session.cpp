#include "session.h"
#include <random>

static std::string genHex(size_t n) {
    static const char* hex = "0123456789abcdef";
    std::random_device rd;
    std::mt19937_64 eng(rd());
    std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFFULL);
    std::string out;
    out.reserve(n*2);
    while (out.size() < n*2) {
        uint64_t v = dist(eng);
        for (int i = 0; i < 16 && out.size() < n*2; ++i) {
            out.push_back(hex[v & 0xF]);
            v >>= 4;
        }
    }
    return out.substr(0, n*2);
}

Session createSessionWithTtl(std::chrono::seconds ttl) {
    Session s;
    s.token = genHex(16);
    s.challenge = genHex(32);
    s.expires = std::chrono::system_clock::now() + ttl;
    s.state = Session::State::Pending;
    s.user_id = 0;
    return s;
}

bool sessionIsExpired(const Session &s) {
    return std::chrono::system_clock::now() > s.expires;
}

void sessionMarkCompleted(Session &s, std::string public_key, long user_id) {
    s.public_key = std::move(public_key);
    s.user_id = user_id;
    s.state = Session::State::Completed;
}
