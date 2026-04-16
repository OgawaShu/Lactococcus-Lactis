#include "CLI.h"
#include <iostream>

CLI::CLI(Client &client) : client_(client) {}

void CLI::run() {
    std::cout << "Client CLI started. Type 'help' for commands.\n";
    std::string cmd;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, cmd)) break;
        if (cmd == "quit" || cmd == "exit") break;
        if (cmd == "help") {
            std::cout << "Commands:\n  invoke - start registration\n  confirm - finish registration\n  quit/exit - leave\n";
            continue;
        }
        if (cmd == "invoke") {
            std::string token, challenge;
            if (client_.invokeRegister(token, challenge)) {
                std::cout << "Token: " << token << "\nChallenge: " << challenge << "\n";
            } else {
                std::cout << "invoke failed\n";
            }
            continue;
        }
        if (cmd == "confirm") {
            std::string token, public_key;
            std::cout << "token: "; std::getline(std::cin, token);
            std::cout << "public_key: "; std::getline(std::cin, public_key);
            long uid;
            if (client_.confirmRegister(token, public_key, uid)) {
                std::cout << "registered user id: " << uid << "\n";
            } else {
                std::cout << "confirm failed\n";
            }
            continue;
        }
        std::cout << "unknown command\n";
    }
}
