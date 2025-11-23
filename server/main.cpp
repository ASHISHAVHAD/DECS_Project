#include "logger.h"
#include "server_app.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    
    if(argc != 2) {
        log_message("Usage: " + std::string(argv[0]) + " <num_server_threads>");
        return 1;
    }

    int num_server_threads = 0;

    try {
        num_server_threads = std::stoi(argv[1]);

        if(num_server_threads <= 0) {
            log_message("Number of threads cannot be negative.");
        }
    }
    catch (const std::invalid_argument& e) {
        // This 'catch' block runs if std::stoi fails because the input was not a number
        // (e.g., you ran `./kv_server abc`).
        log_message("Error: Invalid argument. Number of threads must be an integer.");
        return 1;
    } catch (const std::out_of_range& e) {
        // This 'catch' block runs if the number is too large to fit in an integer,
        // or if our own check for a non-positive number fails.
        log_message("Error: " + std::string(e.what()));
        return 1;
    }

    log_message("Server Process Started.");

    ServerApp app;
    app.init(num_server_threads);
    app.run();
    return 0;
}