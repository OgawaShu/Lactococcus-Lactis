#pragma once

// FILE PURPOSE: This file defines route info related structures and behavior.

#include <cstddef>
#include <string>
#include <vector>

#include "Message.h"

namespace protocol {

struct RouteHop {
    MessageHeader header;
    std::size_t hopIndex = 0;
    std::string nodeId;
    std::string endpoint;
    bool isTerminal = false;
    std::string hopData;
};

struct RouteInfo {
    MessageHeader header;
    std::string chainId;
    std::size_t currentHopIndex = 0;
    std::vector<RouteHop> hops;
};

}  // namespace protocol
