#include "Client.h"
#include <fstream>
#include <sstream>
#include <random>
#include "../lib/httplib.h"
#include "../lib/nlohmann/json.hpp"
#include <vector>
#include <cstring>

// Minimal placeholder ed25519 functions (previously in src/lib/ed25519).
// These are intentionally simple and NOT cryptographically secure. Replace
// with a proper ed25519 library for production.
static void ed25519_create_keypair(unsigned char *pubkey, unsigned char *privkey) {
    std::random_device rd;
    std::mt19937_64 eng(rd());
    for (int i = 0; i < 32; ++i) pubkey[i] = static_cast<unsigned char>(eng() & 0xFF);
    for (int i = 0; i < 64; ++i) privkey[i] = static_cast<unsigned char>(eng() & 0xFF);
}

static void ed25519_sign(unsigned char *sig, const unsigned char *msg, size_t msglen, const unsigned char *privkey, const unsigned char *pubkey) {
    // deterministic-ish placeholder signature
    unsigned char seed = privkey ? privkey[0] : 0;
    for (int i = 0; i < 64; ++i) sig[i] = static_cast<unsigned char>((i + msglen + seed) & 0xFF);
}

Client::Client(const std::string &config_path) : config_path_(config_path) {}

bool Client::loadConfig() {
    std::ifstream ifs(config_path_);
    if (!ifs) return false;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto k = line.substr(0, eq);
        auto v = line.substr(eq + 1);
        if (k == "server_url") server_url_ = v;
        if (k == "storage_path") storage_path_ = v;
    }
    return true;
}

std::string Client::genHex(size_t n) {
    static const char* hex = "0123456789abcdef";
    std::random_device rd;
    std::mt19937_64 eng(rd());
    std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFFULL);
    std::string out;
    out.reserve(n*2);
    while (out.size() < n*2) {
        uint64_t v = dist(eng);
        for (int i = 0; i < 16 && out.size() < n*2; ++i) {
            out.push_back(hex[v & 0xF]);
            v >>= 4;
        }
    }
    return out.substr(0, n*2);
}

bool Client::invokeRegister(std::string &out_token, std::string &out_challenge) {
    out_token = genHex(16);
    out_challenge = genHex(32);
    return true;
}

bool Client::confirmRegister(const std::string &token, const std::string &public_key, long &out_user_id) {
    // stub: write public_key to storage and return a fake id
    std::ofstream ofs(storage_path_ + "/local_keys.txt", std::ios::app);
    if (!ofs) return false;
    ofs << token << '\t' << public_key << '\n';
    out_user_id = 1; // placeholder
    return true;
}

bool Client::registerWithServer(long &out_user_id) {
    if (server_url_.empty()) return false;
    // Step 1: invoke /register/invoke via HTTP
    httplib::Client cli(server_url_.c_str());
    auto res1 = cli.Post("/register/invoke", "", "");
    if (!res1 || res1->status != 200) return false;
    // parse minimal JSON response
    std::string body = res1->body;
    nlohmann::json j = nlohmann::json::parse(body);
    std::string token = j["token"].get<std::string>();
    std::string challenge = j["challenge"].get<std::string>();

    // Step 2: generate ed25519 keypair (using local simple ed25519 stub)
    std::vector<unsigned char> pubkey(32), privkey(64);
    ed25519_create_keypair(pubkey.data(), privkey.data());

    // Step 3: sign challenge
    std::vector<unsigned char> sig(64);
    ed25519_sign(sig.data(), (const unsigned char*)challenge.data(), challenge.size(), privkey.data(), pubkey.data());

    // encode keys/signature as base64-like hex for transport (use hex)
    auto toHex = [](const unsigned char *data, size_t len){
        static const char *hex = "0123456789abcdef";
        std::string s; s.reserve(len*2);
        for (size_t i=0;i<len;++i){ unsigned char v=data[i]; s.push_back(hex[v>>4]); s.push_back(hex[v&0xF]); }
        return s;
    };
    std::string pubkey_hex = toHex(pubkey.data(), pubkey.size());
    std::string sig_hex = toHex(sig.data(), sig.size());

    // Step 4: POST confirmation JSON
    nlohmann::json cj;
    // crude construction using map-like interface
    cj["token"] = token;
    cj["public_key"] = pubkey_hex;
    cj["signed_challenge"] = sig_hex;
    std::string payload = cj.dump();
    auto res2 = cli.Post("/register/confirm", payload, "application/json");
    if (!res2 || res2->status != 200) return false;
    auto rj = nlohmann::json::parse(res2->body);
    std::string idstr = rj["user_id"].get<std::string>();
    out_user_id = std::stol(idstr);

    // persist keys locally
    std::ofstream ofs(storage_path_ + "/local_keys.txt", std::ios::app);
    if (ofs) {
        ofs << pubkey_hex << '\t' << toHex(privkey.data(), 64) << '\n';
    }
    return true;
}
