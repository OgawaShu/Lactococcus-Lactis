#include "Client.h"
#include <fstream>
#include <sstream>
#include <random>

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
