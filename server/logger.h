#ifndef SERVER_LOGGER_H
#define SERVER_LOGGER_H

#include <string>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>

extern std::mutex log_mutex;

void log_message(const std::string& message);

#endif