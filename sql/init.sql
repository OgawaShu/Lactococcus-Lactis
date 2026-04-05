PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS users (
    userId TEXT PRIMARY KEY,
    publicKey TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS password (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    userId TEXT NOT NULL,
    groupId TEXT,
    passwordVerifier TEXT NOT NULL,
    passwordType TEXT NOT NULL CHECK(passwordType IN ('access','duress')),
    FOREIGN KEY (userId) REFERENCES users(userId),
    UNIQUE(userId, groupId, passwordType)
);
