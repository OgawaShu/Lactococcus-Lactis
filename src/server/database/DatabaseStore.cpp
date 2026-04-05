#include "DatabaseStore.h"

#include <filesystem>
#include <fstream>

namespace server::database {

namespace {
std::string ToPasswordTypeText(protocol::PasswordType type) {
    switch (type) {
    case protocol::PasswordType::ACCESS:
        return "decrypt";
    case protocol::PasswordType::DURESS:
        return "coercion";
    default:
        return std::string();
    }
}

bool IsAllowedType(const std::string& type) {
    return type == "decrypt" || type == "coercion";
}
}  // namespace

std::size_t DatabaseStore::PasswordKeyHash::operator()(const PasswordKey& key) const {
    std::hash<std::string> hash;
    return hash(key.userId) ^ (hash(key.groupId) << 1) ^ (hash(key.passwordType) << 2);
}

bool DatabaseStore::Initialize(const std::string& databaseFile, const std::string& initSqlFile, std::string* errorMessage) {
    std::ifstream sql(initSqlFile);
    if (!sql.good()) {
        if (errorMessage != nullptr) {
            *errorMessage = "init sql file not found: " + initSqlFile;
        }
        return false;
    }

    const std::filesystem::path dbPath(databaseFile);
    if (dbPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(dbPath.parent_path(), ec);
        if (ec) {
            if (errorMessage != nullptr) {
                *errorMessage = "cannot create database directory: " + dbPath.parent_path().string();
            }
            return false;
        }
    }

    std::ofstream db(databaseFile, std::ios::app);
    if (!db.good()) {
        if (errorMessage != nullptr) {
            *errorMessage = "cannot open database file: " + databaseFile;
        }
        return false;
    }

    initialized_ = true;
    return true;
}

bool DatabaseStore::SaveRegisterUser(const protocol::user::RegisterUserRequest& request, std::string* errorMessage) {
    if (!initialized_) {
        if (errorMessage != nullptr) {
            *errorMessage = "database is not initialized";
        }
        return false;
    }

    if (request.userId.empty() || request.publicKey.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "userId/publicKey is required";
        }
        return false;
    }

    if (usersById_.count(request.userId) > 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "duplicate userId";
        }
        return false;
    }

    usersById_[request.userId] = request.publicKey;
    return true;
}

bool DatabaseStore::SaveApplyPassword(const protocol::password::ApplyPasswordRequest& request, std::string* errorMessage) {
    if (!initialized_) {
        if (errorMessage != nullptr) {
            *errorMessage = "database is not initialized";
        }
        return false;
    }

    if (usersById_.count(request.userId) == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "foreign key userId does not exist";
        }
        return false;
    }

    if (request.passwordVerifier.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "passwordVerifier is required";
        }
        return false;
    }

    PasswordKey key{request.userId, request.groupId, ToPasswordTypeText(request.passwordType)};
    if (!IsAllowedType(key.passwordType)) {
        if (errorMessage != nullptr) {
            *errorMessage = "invalid passwordType";
        }
        return false;
    }

    if (passwords_.count(key) > 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "duplicate (userId, groupId, passwordType)";
        }
        return false;
    }

    passwords_[key] = request.passwordVerifier;
    return true;
}

}  // namespace server::database
