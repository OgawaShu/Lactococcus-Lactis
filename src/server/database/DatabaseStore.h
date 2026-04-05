#pragma once

#include <string>
#include <unordered_map>

#include "../../common/protocol/password/ApplyPassword.h"
#include "../../common/protocol/user/RegisterUser.h"

namespace server::database {

class DatabaseStore {
public:
    bool Initialize(const std::string& databaseFile, const std::string& initSqlFile, std::string* errorMessage);

    bool SaveRegisterUser(const protocol::user::RegisterUserRequest& request, std::string* errorMessage);
    bool SaveApplyPassword(const protocol::password::ApplyPasswordRequest& request, std::string* errorMessage);

private:
    struct PasswordKey {
        std::string userId;
        std::string groupId;
        std::string passwordType;

        bool operator==(const PasswordKey& other) const {
            return userId == other.userId && groupId == other.groupId && passwordType == other.passwordType;
        }
    };

    struct PasswordKeyHash {
        std::size_t operator()(const PasswordKey& key) const;
    };

    bool initialized_ = false;
    std::unordered_map<std::string, std::string> usersById_;
    std::unordered_map<PasswordKey, std::string, PasswordKeyHash> passwords_;
};

}  // namespace server::database
