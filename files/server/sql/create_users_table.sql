-- SQL to create the `users` table which stores assigned user id and public key
-- Columns:
--  id         : INTEGER PRIMARY KEY AUTOINCREMENT (server-assigned user id)
--  public_key : TEXT NOT NULL (client-provided public key, e.g. PEM or base64)
--  created_at : timestamp when row was created

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    public_key TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
