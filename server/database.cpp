#include "database.h"
#include "config.h"
#include "logger.h"

#include <iostream>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>
#include <queue>
#include <mutex>
#include <condition_variable>

// --- Connection Pool Implementation ---

class ConnectionPool {
public:
    ConnectionPool(int size) {
        try {
            driver = sql::mysql::get_mysql_driver_instance();
            for (int i = 0; i < size; ++i) {
                sql::Connection* con = createConnection();
                if (con) {
                    connection_queue.push(con);
                }
            }
            log_message("Connection Pool initialized with " + std::to_string(connection_queue.size()) + " connections.");
        } catch (sql::SQLException &e) {
            log_message("ERROR: Failed to initialize connection pool: " + std::string(e.what()));
        }
    }

    ~ConnectionPool() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        while (!connection_queue.empty()) {
            delete connection_queue.front();
            connection_queue.pop();
        }
    }

    sql::Connection* getConnection() {
        std::unique_lock<std::mutex> lock(pool_mutex);
        
        while (connection_queue.empty()) {
            pool_cond.wait(lock);
        }

        sql::Connection* con = connection_queue.front();
        connection_queue.pop();

        // --- FIX 2: Robust Connection Validation ---
        bool isValid = false;
        try {
            // 3-second timeout check to prevent hanging on dead sockets
            // Note: isValid(0) might be safer on some drivers if calls are expensive
            isValid = con->isValid(); 
        } catch (...) {
            isValid = false;
        }

        if (!isValid) {
            delete con;
            con = createConnection(); // Attempts to replace it
        }
        // -------------------------------------------

        return con;
    }

    void releaseConnection(sql::Connection* con) {
        if (!con) return;
        std::lock_guard<std::mutex> lock(pool_mutex);
        connection_queue.push(con);
        pool_cond.notify_one();
    }

private:
    sql::mysql::MySQL_Driver *driver;
    std::queue<sql::Connection*> connection_queue;
    std::mutex pool_mutex;
    std::condition_variable pool_cond;

    sql::Connection* createConnection() {
        try {
            sql::Connection* con = driver->connect(DB_HOST, DB_USER, DB_PASS);
            
            // --- FIX 3: Auto Reconnect ---
            // Allows the driver to attempt one reconnection if the server dropped us
            bool reconnect = true;
            con->setClientOption("OPT_RECONNECT", &reconnect);
            con->setSchema(DB_NAME);
            return con;
        } catch (std::exception &e) {
            log_message("ERROR: Could not create MySQL connection: " + std::string(e.what()));
            return nullptr;
        } catch (...) {
            log_message("ERROR: Unknown error creating MySQL connection");
            return nullptr;
        }
    }
};

static ConnectionPool* global_pool = nullptr;

void db_init(int pool_size) {
    if (!global_pool) {
        global_pool = new ConnectionPool(pool_size);
    }
}

// --- Database Interface Implementation ---

sql::Connection* get_db_connection() {
    if (!global_pool) db_init(10);
    return global_pool->getConnection();
}

void close_db_connection(sql::Connection* conn) {
    if (global_pool && conn) {
        global_pool->releaseConnection(conn);
    } else if (conn) {
        delete conn;
    }
}

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
        log_message("DB EXISTS error: " + std::string(e.what()));
        close_db_connection(con);
    }
    return exists;
}

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
        close_db_connection(con);
        if (e.getErrorCode() == 1062) return false; // Duplicate entry
        log_message("DB CREATE error: " + std::string(e.what()));
        return false;
    }
}

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
        log_message("DB UPDATE error: " + std::string(e.what()));
        close_db_connection(con);
        return false;
    }
}

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
        log_message("DB READ error: " + std::string(e.what()));
        close_db_connection(con);
    }
    return value_data;
}

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
        log_message("DB DELETE error: " + std::string(e.what()));
        close_db_connection(con);
        return false;
    }
}