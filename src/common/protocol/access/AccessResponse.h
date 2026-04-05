#pragma once

// FILE PURPOSE: This file defines access response result structures and optional payload container.

#include <optional>
#include <string>

#include "../Message.h"
#include "../ProtocolConstants.h"

namespace protocol {

struct AccessResult {
    MessageHeader header;
    ResultCode resultCode = ResultCode::INTERNAL_ERROR;
    AccessDecision decision = AccessDecision::rejected;
    ProtocolState state = ProtocolState::active;
    bool nextHopRequired = false;
};

struct AccessResponse : Message {
    MessageHeader header;
    AccessResult result;
    std::optional<std::string> payload;
};

}  // namespace protocol
