#include "Handler.h"
#include "../../common/JsonUtils.h"
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
        std::string token;
        std::string public_key;
        size_t pos = 0;
        while (pos < body.size()) {
            auto amp = body.find('&', pos);
            std::string piece = body.substr(pos, (amp==std::string::npos)?std::string::npos:amp-pos);
            auto eq = piece.find('=');
            if (eq != std::string::npos) {
                auto k = piece.substr(0, eq);
                auto v = piece.substr(eq+1);
                if (k == "token") token = v;
                if (k == "public_key") public_key = v;
            }
            if (amp==std::string::npos) break;
            pos = amp + 1;
        }

        if (token.empty() || public_key.empty()) {
            return {400, "{\"status\":\"error\",\"error\":\"missing token or public_key\"}"};
        }

        long id = db.createUser(public_key);
        bool ok = session_manager.complete(token, public_key, id); // Updated for clarity
        if (!ok) return {404, "{\"status\":\"error\",\"error\":\"session not found or expired\"}"};
        std::ostringstream ss;
        ss << "{\"status\":\"ok\",\"user_id\":" << id << "}";
        return {200, ss.str()};
    }

    return {405, "method not allowed"};
}
