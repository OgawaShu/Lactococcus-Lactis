// Client: thin wrapper for client-side operations and configuration
#pragma once

#include <string>

class Client {
public:
    explicit Client(const std::string &config_path);

    // Load configuration from file (server address, storage path, ...)
    bool loadConfig();

    // Request a new registration challenge from server (stubbed).
    // On success returns true and fills out_token/out_challenge.
    bool invokeRegister(std::string &out_token, std::string &out_challenge);

    // Confirm registration with token and public_key. On success returns true and fills out_user_id.
    bool confirmRegister(const std::string &token, const std::string &public_key, long &out_user_id);

    // Full register flow: contact server, obtain token/challenge, generate Ed25519 keypair,
    // sign challenge, send token/public_key/signed_challenge, receive assigned user id.
    // On success returns true and fills out_user_id. Requires server_url_ and storage_path_ set by loadConfig().
    bool registerWithServer(long &out_user_id);

private:
    std::string config_path_;
    std::string server_url_;
    std::string storage_path_;
    std::string ca_cert_path_;

    // Helper: generate hex string
    static std::string genHex(size_t n);
};
