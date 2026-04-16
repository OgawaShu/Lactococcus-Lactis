// Utilities for JSON serialization used by both client and server
#pragma once

#include <string>

namespace common {

// Escape a string for safe insertion into JSON string values.
std::string jsonEscape(const std::string &s);

} // namespace common
