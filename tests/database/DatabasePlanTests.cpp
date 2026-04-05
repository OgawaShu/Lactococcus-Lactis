#include <cassert>
#include <string>

#include "../../src/common/protocol/password/ApplyPassword.h"
#include "../../src/common/protocol/user/RegisterUser.h"
#include "../../src/server/database/DatabaseStore.h"

int main() {
    server::database::DatabaseStore store;
    std::string error;

    assert(store.Initialize("data/test.db", "sql/init.sql", &error));

    protocol::user::RegisterUserRequest registerRequest;
    // RegisterUserRequest stores header-level identity; populate body fields accordingly
    registerRequest.publicKey = "pk-1";
    assert(store.SaveRegisterUser(registerRequest, &error));

    protocol::password::ApplyPasswordRequest apply;
    apply.userId = "u-1";
    apply.groupId = "g-1";
    apply.passwordVerifier = "v-1";
    apply.passwordType = protocol::PasswordType::ACCESS;
    assert(store.SaveApplyPassword(apply, &error));

    protocol::password::ApplyPasswordRequest invalidType;
    invalidType.userId = "u-1";
    invalidType.groupId = "g-2";
    invalidType.passwordVerifier = "v-2";
    invalidType.passwordType = static_cast<protocol::PasswordType>(42);
    assert(!store.SaveApplyPassword(invalidType, &error));

    protocol::password::ApplyPasswordRequest fkFail;
    fkFail.userId = "u-404";
    fkFail.groupId = "g-3";
    fkFail.passwordVerifier = "v-3";
    fkFail.passwordType = protocol::PasswordType::ACCESS;
    assert(!store.SaveApplyPassword(fkFail, &error));

    protocol::password::ApplyPasswordRequest duplicate;
    duplicate.userId = "u-1";
    duplicate.groupId = "g-1";
    duplicate.passwordVerifier = "v-4";
    duplicate.passwordType = protocol::PasswordType::ACCESS;
    assert(!store.SaveApplyPassword(duplicate, &error));

    return 0;
}
