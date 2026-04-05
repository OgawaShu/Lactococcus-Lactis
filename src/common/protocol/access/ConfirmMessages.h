#pragma once

// FILE PURPOSE: This file defines confirm messages related structures and behavior.

#include "../Message.h"
#include "../ProtocolConstants.h"

namespace protocol {

struct RevokeConfirm : Message {
    MessageHeader header;
    ResultCode resultCode = ResultCode::INTERNAL_ERROR;
    ProtocolState state = ProtocolState::active;
};

struct RotateConfirm : Message {
    MessageHeader header;
    ResultCode resultCode = ResultCode::INTERNAL_ERROR;
    ProtocolState state = ProtocolState::active;
};

}  // namespace protocol
