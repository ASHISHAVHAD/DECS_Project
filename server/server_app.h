#ifndef SERVER_APP_H
#define SERVER_APP_H

#include "httplib.h"
#include <string>
#include <iostream>

class ServerApp {
public:
    ServerApp();
    void init(int num_threads);
    void run();

private:
    httplib::Server svr;
    int server_threads;

    std::string extract_value_from_json(const std::string& json_body);
};

#endif