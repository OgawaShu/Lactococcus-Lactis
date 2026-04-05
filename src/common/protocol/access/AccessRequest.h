#pragma once

// FILE PURPOSE: This file defines access request related structures and behavior.

#include <string>

#include "../Message.h"
#include "PasswordInput.h"
#include "RequestContext.h"
#include "RouteInfo.h"

namespace protocol {

struct AccessRequestPayload {
    MessageHeader header;
    std::string encryptedDek;
    PasswordInput passwordInput;
    RouteInfo route;
};

struct AccessRequest : Message {
    MessageHeader header;
    RequestContext context;
    AccessStage stage = AccessStage::initial;
    AccessRequestPayload payload;
};

}  // namespace protocol
