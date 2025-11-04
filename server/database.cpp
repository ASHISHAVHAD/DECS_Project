#include "database.h"
#include "config.h"
#include "logger.h"

#include <iostream>
#include <mysql_driver.h>

// connecting to database
sql::Connection* get_db_connection() {
    sql::Connection *con = nullptr;
    try {
        sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect(DB_HOST, DB_USER, DB_PASS);
        con->setSchema(DB_NAME);
    } catch (sql::SQLException &e) {
        log_message("ERROR: Could not connect to MySQL: " + std::string(e.what()));
        if (con) delete con;
        con = nullptr;
    }
    return con;
}

// disconnect from database
void close_db_connection(sql::Connection* conn) {
    if (conn) {
        delete conn;
    }
}

// called to check if key exists in database
bool db_key_exists(const std::string& key) {
    sql::Connection *con = get_db_connection();
    if (!con) return false;

    bool exists = false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT COUNT(*) FROM key_value_pairs WHERE key_name = ?"));
        pstmt->setString(1, key);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            if (res->getInt(1) > 0) {
                exists = true;
            }
        }
        close_db_connection(con);
    } catch (sql::SQLException &e) {
        log_message("DB EXISTS error for key '" + key + "': " + std::string(e.what()));
        close_db_connection(con);
    }
    return exists;
}

// used for creating and adding a key to database
bool db_create(const std::string& key, const std::string& value) {
    sql::Connection *con = get_db_connection();
    if (!con) return false;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("INSERT INTO key_value_pairs (key_name, value_data) VALUES (?, ?)"));
        pstmt->setString(1, key);
        pstmt->setString(2, value);
        int affected_rows = pstmt->executeUpdate();
        close_db_connection(con);
        return affected_rows > 0;
    } catch (sql::SQLException &e) {
        if (e.getErrorCode() == 1062) {
            close_db_connection(con);
            return false;
        }
        log_message("DB CREATE error for key '" + key + "': " + std::string(e.what()));
        close_db_connection(con);
        return false;
    }
}

// used for updating the value of an existing key in the database
bool db_update(const std::string& key, const std::string& value) {
    sql::Connection *con = get_db_connection();
    if (!con) return false;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE key_value_pairs SET value_data = ? WHERE key_name = ?"));
        pstmt->setString(1, value);
        pstmt->setString(2, key);
        int affected_rows = pstmt->executeUpdate();
        close_db_connection(con);
        return affected_rows > 0;
    } catch (sql::SQLException &e) {
        log_message("DB UPDATE error for key '" + key + "': " + std::string(e.what()));
        close_db_connection(con);
        return false;
    }
}

// used for getting the value associated with a key
std::string db_read(const std::string& key) {
    sql::Connection *con = get_db_connection();
    if (!con) return "";

    std::string value_data = "";
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT value_data FROM key_value_pairs WHERE key_name = ?"));
        pstmt->setString(1, key);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            value_data = res->getString("value_data");
        }
        close_db_connection(con);
    } catch (sql::SQLException &e) {
        log_message("DB READ error for key '" + key + "': " + std::string(e.what()));
        close_db_connection(con);
    }
    return value_data;
}

// used for deleting a key value pair from database
bool db_delete(const std::string& key) {
    sql::Connection *con = get_db_connection();
    if (!con) return false;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("DELETE FROM key_value_pairs WHERE key_name = ?"));
        pstmt->setString(1, key);
        int affected_rows = pstmt->executeUpdate();
        close_db_connection(con);
        return affected_rows > 0;
    } catch (sql::SQLException &e) {
        log_message("DB DELETE error for key '" + key + "': " + std::string(e.what()));
        close_db_connection(con);
        return false;
    }
}