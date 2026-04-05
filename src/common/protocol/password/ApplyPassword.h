#pragma once

// FILE PURPOSE: This file defines apply password messages related structures and behavior.

#include <string>

#include "../Message.h"
#include "../ProtocolConstants.h"

namespace protocol::password {

//invoke by client to begin apply password, get challenge in response to verify user identity
struct BeginApplyPasswordRequest : protocol::Message {
    protocol::MessageHeader header;
    std::string userId;

    const char* MessageTypeName() const override {
        return "BeginApplyPasswordRequest";
    }

    std::string ToJson() const override {
        return "{\"header\":" + header.ToJson() + "," +
               "\"userId\":\"" + userId + "\"}";
    }
};

//response to begin apply password request, contains challenge for client to sign with user's private key.
struct BeginApplyPasswordResponse : protocol::Message {
    protocol::MessageHeader header;
    protocol::ResultCode resultCode;
    std::string applyPasswordChallenge;

    const char* MessageTypeName() const override {
        return "BeginApplyPasswordResponse";
    }

    std::string ToJson() const override {
        return "{\"header\":" + header.ToJson() + "," +
               "\"resultCode\":" + std::to_string(static_cast<int>(resultCode)) + "," +
               "\"applyPasswordChallenge\":\"" + applyPasswordChallenge + "\"}";
    }
};

//invoke by client to complete apply password, contains user's signature of challenge and other info for applying password.
struct ApplyPasswordRequest : protocol::Message {
	protocol::MessageHeader header;
    std::string userId;
    std::string profileId;
    std::string passwordVerifier;
    protocol::PasswordType passwordType;
    std::string groupId;

    const char* MessageTypeName() const override {
        return "ApplyPasswordRequest";
    }

    std::string ToJson() const override {
        const std::string passwordTypeText = passwordType == protocol::PasswordType::DURESS ? "duress" : "access";
        return "{\"header\":" + header.ToJson() + "," +
               "\"userId\":\"" + userId + "\"," +
               "\"profileId\":\"" + profileId + "\"," +
               "\"passwordVerifier\":\"" + passwordVerifier + "\"," +
               "\"passwordType\":\"" + passwordTypeText + "\"," +
               "\"groupId\":\"" + groupId + "\"}";
    }
    bool FromJson(const std::string& json) override {
        auto extract = [&](const std::string& key)->std::string {
            const std::string marker = "\"" + key + "\"";
            auto pos = json.find(marker);
            if (pos == std::string::npos) return std::string();
            auto colon = json.find(':', pos + marker.size());
            if (colon == std::string::npos) return std::string();
            auto start = json.find('"', colon + 1);
            if (start == std::string::npos) return std::string();
            auto end = json.find('"', start + 1);
            if (end == std::string::npos) return std::string();
            return json.substr(start + 1, end - start - 1);
        };
        userId = extract("userId");
        profileId = extract("profileId");
        passwordVerifier = extract("passwordVerifier");
        groupId = extract("groupId");
        const std::string pt = extract("passwordType");
        passwordType = (pt == "duress" || pt == "coercion") ? protocol::PasswordType::DURESS : protocol::PasswordType::ACCESS;
        return !userId.empty() && !passwordVerifier.empty();
    }
};

struct ApplyPasswordResponse : protocol::Message {
    protocol::MessageHeader header;
    protocol::ResultCode resultCode;

    const char* MessageTypeName() const override {
        return "ApplyPasswordResponse";
    }

    std::string ToJson() const override {
        return "{\"header\":" + header.ToJson() + "," +
               "\"resultCode\":" + std::to_string(static_cast<int>(resultCode)) + "}";
    }
};

}  // namespace protocol::password
