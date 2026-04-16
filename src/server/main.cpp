// Minimal HTTPS server using cpp-httplib
// - Build with OpenSSL and define `CPPHTTPLIB_OPENSSL_SUPPORT`
// - App class initializes DB, thread pool and HTTP server. Each incoming
//   request is dispatched to the thread pool for processing.

#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <map>
#include <vector>

#include "../lib/httplib.h"
#include "app/App.h"

int main(int argc, char **argv) {
    const std::string cert = (argc > 1) ? argv[1] : "server.crt";
    const std::string key = (argc > 2) ? argv[2] : "server.key";

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

