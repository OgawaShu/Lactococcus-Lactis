#include "App.h"

#include <iostream>
#include <fstream>
#include <sstream>
// single-threaded blocking mode: remove job tracking and threadpool usage

App::App(const std::string &cert, const std::string &key)
    : db_manager(nullptr), svr(nullptr) {
    // Prepare mutable local paths (start with constructor args)
    std::string cert_path = cert;
    std::string key_path = key;
    std::string dbpath = "files/server/db/server.db";

    // Load server config (optional)
    std::ifstream ifs("files/server/server.config");
    if (ifs) {
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            auto k = line.substr(0, eq);
            auto v = line.substr(eq+1);
            if (k == "db_path") dbpath = v;
            if (k == "cert") cert_path = v;
            if (k == "key") key_path = v;
        }
    }

    db_manager = std::make_unique<DatabaseManager>(dbpath);
    // Initialize database and fail fast if initialization fails
    if (!db_manager->initialize()) {
        throw std::runtime_error("Database initialization failed");
    }
    // Notify successful database initialization
    std::cout << "Database initialized: " << dbpath << "\n";

    // Use SSLServer for TLS. Requires cpp-httplib built with OpenSSL support and linking OpenSSL.
    svr = std::make_unique<httplib::SSLServer>(cert_path.c_str(), key_path.c_str());
    if (!svr->is_valid()) {
        throw std::runtime_error("SSLServer init failed (invalid cert/key or OpenSSL error)");
    }

    svr->set_pre_routing_handler(std::bind(&App::onPreRouting, this, std::placeholders::_1, std::placeholders::_2));
    // create handler instance
    handler = std::make_unique<Handler>(*db_manager, session_manager);
}

httplib::Server::HandlerResponse App::onPreRouting(const httplib::Request &req, httplib::Response &res) {
    std::string method = req.method;
    std::string path = req.path;
    std::string body = req.body;
    std::map<std::string, std::string> headers;
    for (const auto &h : req.headers) headers[h.first] = h.second;

    // Blocking synchronous handling: process request here and return response
    try {
        auto pr = handler->dispatch(method, path, headers, body);
        res.status = pr.first;
        res.set_content(pr.second, "text/plain");
    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content(std::string("internal error: ") + e.what(), "text/plain");
    }
    return httplib::Server::HandlerResponse::Handled;
}

// Session management now delegated to SessionManager

int App::run(const std::string &address, int port) {
    std::cout << "Listening on " << address << ":" << port << "\n";
    if (!svr->listen(address.c_str(), port)) {
        std::cerr << "Listen failed.\n";
        return 1;
    }
    return 0;
}

std::pair<int, std::string> App::handleRequest(const std::string &method,
                                               const std::string &path,
                                               const std::map<std::string, std::string> &headers,
                                               const std::string &body) { 
    std::ostringstream ss;
    ss << method << " " << path << "\n";
    for (const auto &h : headers) ss << h.first << ": " << h.second << "\n";
    ss << "\n";
    ss << body << "\n";
    return {200, ss.str()};
}

// gen_job_id removed in single-threaded blocking mode
