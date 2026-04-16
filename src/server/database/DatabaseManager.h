#pragma once

#include <string>

// DatabaseManager: convenience wrapper around a lightweight file-backed DB
// Provides basic initialization and simple CRUD operations for `users`.
class DatabaseManager {
public:
    explicit DatabaseManager(const std::string &path);

    // Ensure DB is initialized (create tables etc.). Returns true on success.
    bool initialize();

    // Create a new user public key and return assigned id.
    long createUser(const std::string &public_key);

    // Read a user's public key by id. Returns empty string if not found.
    std::string getUserPublicKey(long id);

    // Update a user's public key. Returns true if user existed and was updated.
    bool updateUserPublicKey(long id, const std::string &new_public_key);

    // Delete a user by id. Returns true if user existed and was deleted.
    bool deleteUser(long id);

private:
    std::string path_;
#ifdef HAVE_SQLITE3
    struct sqlite3 *db_ = nullptr;
#endif
};
