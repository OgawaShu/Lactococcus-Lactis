#include "DatabaseManager.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <iomanip>
#ifdef HAVE_SQLITE3
#include <sqlite/sqlite3.h>
#endif
// avoid std::filesystem for compatibility with older MSVC toolset
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

static bool ensure_dir_exists(const std::string &dir) {
    if (dir.empty()) return true;
    std::string cur;
    size_t i = 0;
    // handle absolute paths or drive letters
    if (dir.size() > 1 && dir[1] == ':') {
        cur = dir.substr(0, 2); // e.g., C:
        i = 2;
    }
    while (i < dir.size()) {
        // skip leading separators
        while (i < dir.size() && (dir[i] == '/' || dir[i] == '\\')) {
            if (cur.empty() || cur.back() != '/' ) cur.push_back('\\');
            ++i;
        }
        size_t j = i;
        while (j < dir.size() && dir[j] != '/' && dir[j] != '\\') ++j;
        if (j == i) break;
        std::string part = dir.substr(i, j - i);
        if (!cur.empty() && cur.back() != '\\' && cur.back() != '/') cur.push_back('\\');
        cur += part;
        // try create
#ifdef _WIN32
        if (_mkdir(cur.c_str()) != 0) {
            if (errno != EEXIST) return false;
        }
#else
        if (mkdir(cur.c_str(), 0755) != 0) {
            if (errno != EEXIST) return false;
        }
#endif
        i = j;
    }
    return true;
}

DatabaseManager::DatabaseManager(const std::string &path)
    : path_(path) {
    // Ensure file exists
    std::ofstream ofs(path_, std::ios::app);
    (void)ofs;
}

bool DatabaseManager::initialize() {
    try {
#ifdef HAVE_SQLITE3
        // Ensure parent directory exists
        std::string p = path_;
        auto pos = p.find_last_of("/\\");
        if (pos != std::string::npos) {
            std::string dir = p.substr(0, pos);
            if (!ensure_dir_exists(dir)) return false;
        }

        sqlite3 *db = nullptr;
        if (sqlite3_open(path_.c_str(), &db) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            return false;
        }

        // If SQL schema file exists in repository, execute it to ensure tables
        std::ifstream sqlifs("files/server/sql/create_users_table.sql");
        if (sqlifs) {
            std::ostringstream ss;
            ss << sqlifs.rdbuf();
            std::string sql = ss.str();
            if (!sql.empty()) {
                char *errmsg = nullptr;
                int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
                if (rc != SQLITE_OK) {
                    if (errmsg) sqlite3_free(errmsg);
                    sqlite3_close(db);
                    return false;
                }
            }
        }

        sqlite3_close(db);
        return true;
#else
        // determine parent directory from path_
        std::string p = path_;
        auto pos = p.find_last_of("/\\");
        if (pos != std::string::npos) {
            std::string dir = p.substr(0, pos);
            if (!ensure_dir_exists(dir)) return false;
        }
        // ensure file exists
        std::ofstream ofs(path_, std::ios::app);
        if (!ofs) return false;
        return true;
#endif
    } catch (...) {
        return false;
    }
}

long DatabaseManager::createUser(const std::string &public_key) {
#ifdef HAVE_SQLITE3
    // Use SQLite: open per-operation, insert row, return last_insert_rowid
    // ensure parent directory exists
    std::string p = path_;
    auto pos = p.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string dir = p.substr(0, pos);
        if (!ensure_dir_exists(dir)) throw std::runtime_error("Failed to create DB directory: " + dir);
    }

    sqlite3 *db = nullptr;
    if (sqlite3_open(path_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        throw std::runtime_error("Failed to open SQLite DB: " + path_);
    }

    // created_at in ISO 8601
    std::time_t now = std::time(nullptr);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    std::ostringstream ts;
    ts << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    const char *sql = "INSERT INTO users(public_key, created_at) VALUES(?, ?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare INSERT statement");
    }
    sqlite3_bind_text(stmt, 1, public_key.c_str(), -1, SQLITE_TRANSIENT);
    std::string tsstr = ts.str();
    sqlite3_bind_text(stmt, 2, tsstr.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        throw std::runtime_error("Failed to execute INSERT statement");
    }
    sqlite3_finalize(stmt);
    long nid = static_cast<long>(sqlite3_last_insert_rowid(db));
    sqlite3_close(db);
    return nid;
#else
    // Determine next id by counting lines
    std::ifstream ifs(path_);
    long maxid = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        long id = std::stol(line.substr(0, tab));
        if (id > maxid) maxid = id;
    }
    long nid = maxid + 1;

    // created_at in ISO 8601
    std::time_t now = std::time(nullptr);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    std::ostringstream ts;
    ts << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    std::ofstream ofs(path_, std::ios::app);
    if (!ofs) throw std::runtime_error("Failed to write DB file: " + path_);
    ofs << nid << "\t" << public_key << "\t" << ts.str() << "\n";
    return nid;
#endif
}

std::string DatabaseManager::getUserPublicKey(long id) {
#ifdef HAVE_SQLITE3
    sqlite3 *db = nullptr;
    if (sqlite3_open(path_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return std::string();
    }
    const char *sql = "SELECT public_key FROM users WHERE id = ? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        sqlite3_close(db);
        return std::string();
    }
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(id));
    std::string result;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        if (text) result = reinterpret_cast<const char *>(text);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
#else
    std::ifstream ifs(path_);
    if (!ifs) return std::string();
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        long rid = std::stol(line.substr(0, tab1));
        if (rid != id) continue;
        auto tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) continue;
        return line.substr(tab1 + 1, tab2 - tab1 - 1);
    }
    return std::string();
#endif
}

bool DatabaseManager::updateUserPublicKey(long id, const std::string &new_public_key) {
#ifdef HAVE_SQLITE3
    sqlite3 *db = nullptr;
    if (sqlite3_open(path_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    const char *sql = "UPDATE users SET public_key = ? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, new_public_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(id));
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    bool ok = (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    sqlite3_close(db);
    return ok;
#else
    // Read whole file, rewrite with update
    std::ifstream ifs(path_);
    if (!ifs) return false;
    std::ostringstream out;
    bool found = false;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        long rid = std::stol(line.substr(0, tab1));
        if (rid == id) {
            // replace public_key and keep timestamp
            auto tab2 = line.find('\t', tab1 + 1);
            if (tab2 == std::string::npos) continue;
            std::string ts = line.substr(tab2 + 1);
            out << id << '\t' << new_public_key << '\t' << ts << '\n';
            found = true;
        } else {
            out << line << '\n';
        }
    }
    ifs.close();
    if (!found) return false;
    std::ofstream ofs(path_, std::ios::trunc);
    if (!ofs) return false;
    ofs << out.str();
    return true;
#endif
}

bool DatabaseManager::deleteUser(long id) {
#ifdef HAVE_SQLITE3
    sqlite3 *db = nullptr;
    if (sqlite3_open(path_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    const char *sql = "DELETE FROM users WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(id));
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    bool ok = (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    sqlite3_close(db);
    return ok;
#else
    std::ifstream ifs(path_);
    if (!ifs) return false;
    std::ostringstream out;
    bool found = false;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        long rid = std::stol(line.substr(0, tab1));
        if (rid == id) {
            found = true;
            continue; // skip
        }
        out << line << '\n';
    }
    ifs.close();
    if (!found) return false;
    std::ofstream ofs(path_, std::ios::trunc);
    if (!ofs) return false;
    ofs << out.str();
    return true;
#endif
}
