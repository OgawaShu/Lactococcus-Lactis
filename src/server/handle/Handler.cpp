#include "Handler.h"
#include "../../common/JsonUtils.h"
#include "../../lib/nlohmann/json.hpp"
#include <sstream>

// JSON escaping provided by common::jsonEscape

Handler::Handler(DatabaseManager &db, SessionManager &sm) : db(db), session_manager(sm) {}

std::pair<int, std::string> Handler::dispatch(const std::string &method,
                                               const std::string &path,
                                               const std::map<std::string, std::string> &headers,
                                               const std::string &body) {
    if (path.rfind("/register", 0) == 0) {
        return handleRegister(method, path, headers, body);
    }
    std::string resp = "{\"status\":\"error\",\"error\":\"unhandled request\"}";
    return {404, resp};
}
std::pair<int, std::string> Handler::handleRegister(const std::string &method,
                                                    const std::string &path,
                                                    const std::map<std::string, std::string> &headers,
                                                    const std::string &body) {
    if (method == "POST" && path == "/register/invoke") {
        auto s = session_manager.create(std::chrono::seconds(300));
        std::ostringstream ss;
        ss << "{\"status\":\"ok\",";
        ss << "\"token\":\"" << common::jsonEscape(s->token) << "\",";
        ss << "\"challenge\":\"" << common::jsonEscape(s->challenge) << "\"}";
        return {200, ss.str()};
    }

    if (method == "POST" && path == "/register/confirm") {
        // Expect JSON body: { "token": "...", "public_key": "...", "signed_challenge": "..." }
        try {
            auto j = nlohmann::json::parse(body);
            std::string token = j["token"].get<std::string>();
            std::string public_key = j["public_key"].get<std::string>();
            // signed_challenge may be used later; ensure presence
            std::string signed_challenge = j["signed_challenge"].get<std::string>();

            if (token.empty() || public_key.empty() || signed_challenge.empty()) {
                return {400, "{\"status\":\"error\",\"error\":\"missing fields\"}"};
            }

            long id = db.createUser(public_key);
            bool ok = session_manager.complete(token, public_key, id);
            if (!ok) return {404, "{\"status\":\"error\",\"error\":\"session not found or expired\"}"};
            std::ostringstream ss;
            ss << "{\"status\":\"ok\",\"user_id\":" << id << "}";
            return {200, ss.str()};
        } catch (...) {
            return {400, "{\"status\":\"error\",\"error\":\"invalid json\"}"};
        }
    }

    return {405, "method not allowed"};
}
