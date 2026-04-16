// Minimal HTTPS server using cpp-httplib
// - Build with OpenSSL and define `CPPHTTPLIB_OPENSSL_SUPPORT`
// - App class initializes DB, thread pool and HTTP server. Each incoming
//   request is dispatched to the thread pool for processing.

#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <map>
#include <vector>

#include <httplib/httplib.h>
#include "app/App.h"

int main(int argc, char **argv) {
    // Load defaults from config file if present
    std::map<std::string, std::string> cfg;
    std::ifstream ifs("files/server/server.config");
    if (ifs) {
        std::string line;
        auto trim = [](std::string s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
            return s;
        };
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            // skip comments
            if (line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string k = trim(line.substr(0, pos));
            std::string v = trim(line.substr(pos + 1));
            if (!k.empty()) cfg[k] = v;
        }
    }

    const std::string cert = (argc > 1) ? argv[1] : (cfg.count("cert") ? cfg["cert"] : "server.crt");
    const std::string key = (argc > 2) ? argv[2] : (cfg.count("key") ? cfg["key"] : "server.key");

#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    std::cerr << "Error: cpp-httplib must be built with OpenSSL support. Define CPPHTTPLIB_OPENSSL_SUPPORT and link OpenSSL.\n";
    return 2;
#endif

    try {
        App app(cert, key);
        return app.run();
    } catch (const std::exception &e) {
        std::cerr << "Failed to start app: " << e.what() << "\n";
        return 3;
    }
}

