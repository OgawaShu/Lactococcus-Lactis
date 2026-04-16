#include "CLI.h"
#include "Client.h"
#include <iostream>

int main(int argc, char **argv) {
    const std::string config = (argc > 1) ? argv[1] : "files/client/client.config";
    Client client(config);
    if (!client.loadConfig()) {
        std::cerr << "Failed to load client config: " << config << "\n";
        return 1;
    }
    CLI cli(client);
    cli.run();
    return 0;
}
