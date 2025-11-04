#include "logger.h"
#include "server_app.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        log_message("Usage: " + std::string(argv[0]) + " <num_server_threads>");
        return 1;
    }

    int num_server_threads = 0;
    try {
        num_server_threads = std::stoi(argv[1]);
        if (num_server_threads <= 0) {
            throw std::out_of_range("Number of threads must be positive.");
        }
    } catch (const std::invalid_argument& e) {
        log_message("ERROR: Invalid number of threads provided: " + std::string(argv[1]) + ". Must be an integer.");
        return 1;
    } catch (const std::out_of_range& e) {
        log_message("ERROR: Number of threads out of range: " + std::string(e.what()));
        return 1;
    }

    ServerApp app;
    app.init(num_server_threads);
    app.run();
    return 0;
}