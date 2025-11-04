#ifndef SERVER_DATABASE_H
#define SERVER_DATABASE_H

#include <string>
#include <memory>

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

sql::Connection* get_db_connection();
void close_db_connection(sql::Connection* conn);

bool db_key_exists(const std::string& key);
bool db_create(const std::string& key, const std::string& value);
bool db_update(const std::string& key, const std::string& value);
std::string db_read(const std::string& key);
bool db_delete(const std::string& key);

#endif