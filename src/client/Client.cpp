#include "Client.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <random>
#include <httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <cstring>
#if defined(HAVE_LIBSODIUM)
#include <sodium.h>
#else
// libsodium not available: provide minimal placeholders so the client can
// build and run for development. These implementations are NOT cryptographically
// secure and must NOT be used in production. Define HAVE_LIBSODIUM to use the
// real libsodium implementation and link the library.
#include <random>

// ed25519 signature size
#ifndef crypto_sign_BYTES
#define crypto_sign_BYTES 64
#endif

static int sodium_init() {
    return 1; // pretend initialization succeeded
}

static void crypto_sign_ed25519_keypair(unsigned char *pk, unsigned char *sk) {
    std::random_device rd;
    std::mt19937_64 eng(rd());
    for (size_t i = 0; i < 32; ++i) pk[i] = static_cast<unsigned char>(eng() & 0xFF);
    for (size_t i = 0; i < 64; ++i) sk[i] = static_cast<unsigned char>(eng() & 0xFF);
}

static int crypto_sign_detached(unsigned char *sig, unsigned long long *siglen,
                                const unsigned char *m, unsigned long long mlen,
                                const unsigned char *sk) {
    uint64_t seed = 1469598103934665603ULL;
    for (unsigned long long i = 0; i < mlen; ++i) seed = (seed ^ m[i]) * 1099511628211ULL;
    for (size_t i = 0; i < 32; ++i) seed = (seed ^ sk[i]) * 1099511628211ULL;
    std::mt19937_64 eng(seed);
    for (size_t i = 0; i < crypto_sign_BYTES; ++i) sig[i] = static_cast<unsigned char>(eng() & 0xFF);
    if (siglen) *siglen = crypto_sign_BYTES;
    return 0;
}
#endif

// Use libsodium for ed25519 operations. Requires linking with libsodium and
// that sodium_init() succeeds before use. These wrappers match the previous
// simple API: pubkey (32 bytes), privkey (64 bytes), sig (64 bytes).
static void ed25519_create_keypair(unsigned char *pubkey, unsigned char *privkey) {
    static bool inited = false;
    if (!inited) {
        if (sodium_init() < 0) {
            // initialization failed; zero out keys to avoid uninitialized data
            std::memset(pubkey, 0, 32);
            std::memset(privkey, 0, 64);
            return;
        }
        inited = true;
    }

    // libsodium provides crypto_sign_ed25519_keypair
    crypto_sign_ed25519_keypair(pubkey, privkey);
}

static void ed25519_sign(unsigned char *sig, const unsigned char *msg, size_t msglen, const unsigned char *privkey, const unsigned char * /*pubkey*/) {
    static bool inited = false;
    if (!inited) {
        if (sodium_init() < 0) {
            std::memset(sig, 0, crypto_sign_BYTES);
            return;
        }
        inited = true;
    }

    unsigned long long siglen = 0;
    // Use detached signature API
    crypto_sign_detached(sig, &siglen, msg, static_cast<unsigned long long>(msglen), privkey);
    (void)siglen; // siglen should equal crypto_sign_BYTES
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
        if (k == "cert") ca_cert_path_ = v;
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
    if (server_url_.empty()) return false;
    // parse server_url_ to determine scheme, host and port
    std::string url = server_url_;
    bool is_ssl = false;
    if (url.rfind("https://", 0) == 0) {
        is_ssl = true;
        url = url.substr(8);
    } else if (url.rfind("http://", 0) == 0) {
        url = url.substr(7);
    }
    // strip any path
    auto slash = url.find('/');
    if (slash != std::string::npos) url = url.substr(0, slash);
    std::string host = url;
    int port = is_ssl ? 443 : 80;
    auto colon = host.find(':');
    if (colon != std::string::npos) {
        port = std::stoi(host.substr(colon + 1));
        host = host.substr(0, colon);
    }

    httplib::Result res;
#if defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    if (is_ssl) {
        // If host is an IPv4 address and CA cert is provided, use 'localhost'
        // as the SNI/host for SSLClient so certificate hostname checks
        // against a cert issued to 'localhost' succeed in common dev setups.
        bool host_is_ipv4 = true;
        for (char c : host) if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.')) { host_is_ipv4 = false; break; }
        std::string sni_host = host;
        if (host_is_ipv4 && !ca_cert_path_.empty()) sni_host = "localhost";

        httplib::SSLClient cli(sni_host.c_str(), port);
        // If a CA cert path was provided in config, use it to validate the
        // server certificate (useful for self-signed certs in dev).
        if (!ca_cert_path_.empty()) {
            std::ifstream caf(ca_cert_path_);
            if (!caf) {
                std::cerr << "[client] CA cert file not found or unreadable: " << ca_cert_path_ << "\n";
            } else {
                std::cerr << "[client] using CA cert file: " << ca_cert_path_ << "\n";
                cli.set_ca_cert_path(ca_cert_path_.c_str());
            }
        }
        std::cerr << "[client] contacting " << host << ":" << port << " SSL=" << is_ssl << " sni=" << sni_host << " ca=" << ca_cert_path_ << "\n";
        res = cli.Post("/register/invoke", "", "");
    } else {
        httplib::Client cli(host.c_str(), port);
        std::cerr << "[client] contacting " << host << ":" << port << " SSL=" << is_ssl << "\n";
        res = cli.Post("/register/invoke", "", "");
    }
#else
    if (is_ssl) return false; // no SSL support compiled into httplib
    httplib::Client cli(host.c_str(), port);
    std::cerr << "[client] contacting " << host << ":" << port << " SSL=" << is_ssl << " (no SSL support)\n";
    res = cli.Post("/register/invoke", "", "");
#endif
    if (!res) {
        std::cerr << "[client] no response from server\n";
        return false;
    }
    if (res->status != 200) {
        std::cerr << "[client] server returned status=" << res->status << " body=" << res->body << "\n";
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(res->body);
        out_token = j["token"].get<std::string>();
        out_challenge = j["challenge"].get<std::string>();
        return true;
    } catch (...) {
        return false;
    }
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
    std::string token, challenge;
    if (!invokeRegister(token, challenge)) return false;

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
    cj["token"] = token;
    cj["public_key"] = pubkey_hex;
    cj["signed_challenge"] = sig_hex;
    std::string payload = cj.dump();

    // send to server using same host/port parsing as invokeRegister
    std::string url = server_url_;
    bool is_ssl = false;
    if (url.rfind("https://", 0) == 0) {
        is_ssl = true;
        url = url.substr(8);
    } else if (url.rfind("http://", 0) == 0) {
        url = url.substr(7);
    }
    auto slash = url.find('/');
    if (slash != std::string::npos) url = url.substr(0, slash);
    std::string host = url;
    int port = is_ssl ? 443 : 80;
    auto colon = host.find(':');
    if (colon != std::string::npos) {
        port = std::stoi(host.substr(colon + 1));
        host = host.substr(0, colon);
    }

    httplib::Result res2;
#if defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    if (is_ssl) {
        bool host_is_ipv4 = true;
        for (char c : host) if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.')) { host_is_ipv4 = false; break; }
        std::string sni_host = host;
        if (host_is_ipv4 && !ca_cert_path_.empty()) sni_host = "localhost";

        httplib::SSLClient cli(sni_host.c_str(), port);
        if (!ca_cert_path_.empty()) {
            std::ifstream caf(ca_cert_path_);
            if (!caf) {
                std::cerr << "[client] CA cert file not found or unreadable: " << ca_cert_path_ << "\n";
            } else {
                std::cerr << "[client] using CA cert file: " << ca_cert_path_ << "\n";
                cli.set_ca_cert_path(ca_cert_path_.c_str());
            }
        }
        std::cerr << "[client] POST /register/confirm to " << host << ":" << port << " SSL=" << is_ssl << " sni=" << sni_host << " ca=" << ca_cert_path_ << "\n";
        res2 = cli.Post("/register/confirm", payload, "application/json");
    } else {
        httplib::Client cli(host.c_str(), port);
        std::cerr << "[client] POST /register/confirm to " << host << ":" << port << " SSL=" << is_ssl << "\n";
        res2 = cli.Post("/register/confirm", payload, "application/json");
    }
#else
    if (is_ssl) return false; // no SSL support compiled into httplib
    httplib::Client cli(host.c_str(), port);
    res2 = cli.Post("/register/confirm", payload, "application/json");
#endif
    if (!res2) {
        std::cerr << "[client] no response from server (confirm)\n";
        return false;
    }
    if (res2->status != 200) {
        std::cerr << "[client] server returned status=" << res2->status << " body=" << res2->body << "\n";
        return false;
    }
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
