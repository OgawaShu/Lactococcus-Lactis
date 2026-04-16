// App class declaration
#pragma once

#include <string>
#include <map>

#include "../lib/httplib.h"
#include "../database/DatabaseManager.h"
#include "../auth/Session.h"
#include "../auth/SessionManager.h"
#include "../handle/Handler.h"
#include <memory>

class App {
public:
    App(const std::string &cert, const std::string &key);
    int run(const std::string &address = "127.0.0.1", int port = 8443);

private:
    // Pre-routing handler called by cpp-httplib for each request. Implemented
    // in the .cpp to avoid inline definitions in the header.
    httplib::Server::HandlerResponse onPreRouting(const httplib::Request &req, httplib::Response &res);

    std::pair<int, std::string> handleRequest(const std::string &method,
                                               const std::string &path,
                                               const std::map<std::string, std::string> &headers,
                                               const std::string &body);

    std::unique_ptr<DatabaseManager> db_manager;
    std::unique_ptr<httplib::SSLServer> svr;
    // Session manager
    SessionManager session_manager;
    std::unique_ptr<class Handler> handler;
};
