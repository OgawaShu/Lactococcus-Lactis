#pragma once

#include "Client.h"
#include <string>

class CLI {
public:
    explicit CLI(Client &client);
    void run();

private:
    Client &client_;
};
