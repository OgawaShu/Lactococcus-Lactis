#include "SessionManager.h"
#include <algorithm>

std::shared_ptr<Session> SessionManager::create(std::chrono::seconds ttl) {
    auto tmp = createSessionWithTtl(ttl);
    auto s = std::make_shared<Session>(std::move(tmp));
    std::lock_guard<std::mutex> lk(mu_);
    sessions_[s->token] = s;
    return s;
}

std::shared_ptr<Session> SessionManager::find(const std::string &token) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return nullptr;
    auto s = it->second;
    if (sessionIsExpired(*s)) {
        sessions_.erase(it);
        return nullptr;
    }
    return s;
}

bool SessionManager::complete(const std::string &token, const std::string &public_key, long user_id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return false;
    auto s = it->second;
    if (sessionIsExpired(*s)) { sessions_.erase(it); return false; }
    sessionMarkCompleted(*s, public_key, user_id);
    return true;
}

void SessionManager::sweep() {
    std::lock_guard<std::mutex> lk(mu_);
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> to_remove;
    for (auto &p : sessions_) {
        if (sessionIsExpired(*p.second)) to_remove.push_back(p.first);
    }
    for (auto &k : to_remove) sessions_.erase(k);
}
