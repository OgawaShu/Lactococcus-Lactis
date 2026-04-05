#pragma once

// FILE PURPOSE: This file defines request context related structures and behavior.

#include <optional>
#include <string>

#include "Message.h"

namespace protocol {

struct RequestContext {
    MessageHeader header;
    std::string fileId;
    std::optional<std::string> fileGroupId;
    std::string clientId;
    std::string configId;
};

}  // namespace protocol
