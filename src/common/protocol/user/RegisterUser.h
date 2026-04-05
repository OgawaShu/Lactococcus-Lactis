#pragma once

// FILE PURPOSE: This file defines register user request and response protocol models.

#include <string>
// Use real nlohmann/json if available; otherwise use local shim via Message.h include

#include "../Message.h"
#include "../ProtocolConstants.h"

namespace protocol::user {
//invoke by client to begin register user, get registration challenge in response.
struct BeginRegisterUserRequest : protocol::Message {
    protocol::MessageHeader header;

    const char* MessageTypeName() const override {
        return "BeginRegisterUserRequest";
    }
    std::string ToJson() const override {
        return "{\"header\":" + header.ToJson() + "}";
    }
    bool FromJson(const std::string& /*json*/) override {
        // BeginRegisterUserRequest has no body fields in current protocol
        return true;
    }
};

//response to begin register user request, assign user id and return registration challenge for client to sign with user's private key.
struct BeginRegisterUserResponse : protocol::Message {
    protocol::MessageHeader header;
    protocol::ResultCode resultCode;
    std::string userId; // assigned by server

    const char* MessageTypeName() const override {
        return "BeginRegisterUserResponse";
    }

    std::string ToJson() const override {
        // registration challenge lives in header.auth_challenge (base64 encoded by header.ToJson())
        return std::string("{\"header\":") + header.ToJson() + "," +
               std::string("\"resultCode\":") + std::to_string(static_cast<int>(resultCode)) + "," +
               std::string("\"userId\":\"") + userId + "\"}";
    }
    bool FromJson(const std::string& json) override {
        // first extract nested header object (if present) and parse it
        try {
            auto j = nlohmann::json::parse(json);
            if (j.contains("header") && j["header"].is_object()) {
                header = header.FromJson(j["header"].dump());
            }
            if (j.contains("userId") && j["userId"].is_string()) userId = j["userId"].get<std::string>();
        // try parse resultCode (numeric) if present
            if (j.contains("resultCode") && j["resultCode"].is_number_integer()) resultCode = static_cast<protocol::ResultCode>(j["resultCode"].get<int>());
        } catch (...) {
            return false;
        }
        return !userId.empty();
    }
};

//invoke by client to complete register user, contains user's signature of registration challenge and user's public key.
struct RegisterUserRequest : protocol::Message {
    protocol::MessageHeader header;
    std::string userId;
    std::string publicKey;

    const char* MessageTypeName() const override {
        return "RegisterUserRequest";
    }

    std::string ToJson() const override {
        return std::string("{\"header\":") + header.ToJson() + "," +
               std::string("\"userId\":\"") + userId + "\"," +
               std::string("\"publicKey\":\"") + publicKey + "\"}";
    }
    bool FromJson(const std::string& json) override {
        try {
            auto j = nlohmann::json::parse(json);
            if (j.contains("header") && j["header"].is_object()) {
                header = header.FromJson(j["header"].dump());
            }
            if (j.contains("userId") && j["userId"].is_string()) userId = j["userId"].get<std::string>();
            if (j.contains("publicKey") && j["publicKey"].is_string()) publicKey = j["publicKey"].get<std::string>();
            // registration proof removed; validation occurs via header.auth_challenge
        } catch (...) {
            return false;
        }
        return !userId.empty() && !publicKey.empty();
    }
};

//response to register user request, confirm the registration.
struct RegisterUserResponse : protocol::Message {
    protocol::MessageHeader header;
    protocol::ResultCode resultCode;

    const char* MessageTypeName() const override {
        return "RegisterUserResponse";
    }

    std::string ToJson() const override {
        return std::string("{\"header\":") + header.ToJson() + "," +
            std::string("\"resultCode\":") + std::to_string(static_cast<int>(resultCode)) + "}";
    }
    bool FromJson(const std::string& json) override {
        try {
            auto j = nlohmann::json::parse(json);
            if (j.contains("header") && j["header"].is_object()) {
                header = header.FromJson(j["header"].dump());
            }
            if (j.contains("resultCode") && j["resultCode"].is_number_integer()) {
                resultCode = static_cast<protocol::ResultCode>(j["resultCode"].get<int>());
            }
        } catch (...) {
            return false;
        }
        return true;
    }
};

}  // namespace protocol::user
