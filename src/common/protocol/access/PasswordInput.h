#pragma once

// FILE PURPOSE: This file defines password input related structures and behavior.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../Message.h"

namespace protocol {

enum class PasswordType {
    decrypt = 0,
    coercion
};

struct HopPassword {
    MessageHeader header;
    std::size_t hopIndex = 0;
    std::string password;
    std::optional<PasswordType> passwordType;
};

struct PasswordInput {
    MessageHeader header;
    std::vector<HopPassword> hopPasswords;
};

}  // namespace protocol
