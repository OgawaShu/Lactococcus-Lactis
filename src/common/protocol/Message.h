#pragma once

// FILE PURPOSE: This file defines message header related structures and behavior.

#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <vector>
#include <sstream>
#include <memory>
#include <algorithm>
#include <cstring>
// JSON parsing library (header-only).
#include <nlohmann/json.hpp>

namespace protocol {

inline std::string Base64Encode(const uint8_t* data, size_t len) {
    static const char* basis = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= len) {
        uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i+1]) << 8) |
                     static_cast<uint32_t>(data[i+2]);
        out.push_back(basis[(v >> 18) & 0x3F]);
        out.push_back(basis[(v >> 12) & 0x3F]);
        out.push_back(basis[(v >> 6) & 0x3F]);
        out.push_back(basis[v & 0x3F]);
        i += 3;
    }
    if (i < len) {
        uint32_t v = 0;
        int remaining = static_cast<int>(len - i);
        for (int j = 0; j < remaining; ++j) {
            v = (v << 8) | data[i++];
        }
        v <<= (3 - remaining) * 8;
        out.push_back(basis[(v >> 18) & 0x3F]);
        out.push_back(basis[(v >> 12) & 0x3F]);
        out.push_back(remaining >= 2 ? basis[(v >> 6) & 0x3F] : '=');
        out.push_back(remaining >= 3 ? basis[v & 0x3F] : '=');
    }
    return out;
}

inline std::vector<uint8_t> Base64Decode(const std::string& in) {
    static int T[256];
    static bool init = false;
    if (!init) {
        std::fill(std::begin(T), std::end(T), -1);
        const char* basis = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; basis[i]; ++i) T[static_cast<unsigned char>(basis[i])] = i;
        T[static_cast<unsigned char>('=')] = 0;
        init = true;
    }
    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        int d = T[c];
        if (d == -1) continue;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

} // namespace protocol

#include "ProtocolConstants.h"

namespace protocol {

struct MessageHeader {
    std::string protocolVersion;
    std::string requestId;
    std::string traceId;
    std::array<std::uint8_t, 32> auth_challenge;
    std::vector<uint8_t> auth_signature; // client's signature over challenge+context (base64 in JSON)
    std::int64_t timestamp = 0;

    std::string ToJson() const {
        nlohmann::json j;
        j["protocolVersion"] = protocolVersion;
        j["requestId"] = requestId;
        j["traceId"] = traceId;
        j["auth"] = Base64Encode(auth_challenge.data(), auth_challenge.size());
        // auth_signature is binary; serialize as base64 string under "auth_sig"
        if (!auth_signature.empty()) {
            j["auth_sig"] = Base64Encode(auth_signature.data(), auth_signature.size());
        } else {
            j["auth_sig"] = std::string();
        }
        j["timestamp"] = timestamp;
        return j.dump();
    }
    // Populate header from a JSON object (very small parser). Decodes base64 auth.
    MessageHeader FromJson(const std::string& json) {
        try {
            auto j = nlohmann::json::parse(json);
            if (j.contains("protocolVersion") && j["protocolVersion"].is_string()) protocolVersion = j["protocolVersion"].get<std::string>();
            if (j.contains("requestId") && j["requestId"].is_string()) requestId = j["requestId"].get<std::string>();
            if (j.contains("traceId") && j["traceId"].is_string()) traceId = j["traceId"].get<std::string>();
            if (j.contains("timestamp") && j["timestamp"].is_number_integer()) timestamp = j["timestamp"].get<std::int64_t>();
            if (j.contains("auth") && j["auth"].is_string()) {
                const std::string auth_b64 = j["auth"].get<std::string>();
                auto decoded = Base64Decode(auth_b64);
                size_t copyLen = std::min(decoded.size(), auth_challenge.size());
                std::fill(auth_challenge.begin(), auth_challenge.end(), 0);
                std::copy_n(decoded.begin(), copyLen, auth_challenge.begin());
            }
            if (j.contains("auth_sig") && j["auth_sig"].is_string()) {
                const std::string sig_b64 = j["auth_sig"].get<std::string>();
                auth_signature = Base64Decode(sig_b64);
            }
        } catch (...) {
            // on parse error leave defaults
        }
        return *this;
    }
};

struct Message {
    virtual ~Message() = default;
    virtual const char* MessageTypeName() const = 0;
    virtual std::string ToJson() const = 0;
    // Populate this message from a JSON string. Default implementation
    // returns false indicating not implemented for this type.
    virtual bool FromJson(const std::string& json) { (void)json; return false; }
    MessageHeader header;
};

// Factory: create message instance by type name and populate from json.
std::unique_ptr<Message> CreateMessageFromType(const std::string& typeName, const std::string& json);

}  // namespace protocol
