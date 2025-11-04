#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>

// server parameters
const int SERVER_PORT = 8080;
const int MAX_CACHE_SIZE = 100;

// MySQL Database Configuration
const std::string DB_HOST = "localhost";
const std::string DB_USER = "kv_user";
const std::string DB_PASS = "Tornado123&";
const std::string DB_NAME = "kv_store";

#endif