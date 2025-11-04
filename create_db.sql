CREATE DATABASE IF NOT EXISTS kv_store; -- creating database

CREATE USER IF NOT EXISTS 'kv_user'@'localhost' IDENTIFIED BY 'Tornado123&'; -- creating new user

GRANT ALL PRIVILEGES ON kv_store.* TO 'kv_user'@'localhost'; -- granting privileges

FLUSH PRIVILEGES;

USE kv_store;

CREATE TABLE IF NOT EXISTS key_value_pairs (
    key_name VARCHAR(255) PRIMARY KEY,
    value_data TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);