#pragma once

// FILE PURPOSE: This file defines protocol constants related structures and behavior.

namespace protocol {


enum class PasswordType {
    ACCESS = 0,
    DURESS
};

enum class ResultCode {
    OK = 0,
    REJECTED,
    UNAUTHORIZED,
    REVOKED,
    EXPIRED,
    INVALID_REQUEST,
    INTERNAL_ERROR
};

enum class ProtocolState {
    active = 0,
    rotated,
    expired,
    revoked
};

enum class AccessDecision {
    forward = 0,
    revoked,
    rejected
};

enum class AccessStage {
    initial = 0,
    forward
};

}  // namespace protocol
