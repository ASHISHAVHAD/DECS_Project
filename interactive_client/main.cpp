#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "../httplib.h"

const std::string SERVER_HOST = "127.0.0.1"; // can change this if server ip is also changed
const int SERVER_PORT = 8080;

// sends one http request to the server and gets response
std::string send_http_request(const std::string& method, const std::string& key, const std::string& value_body = "") {
    httplib::Client cli(SERVER_HOST, SERVER_PORT);
    cli.set_connection_timeout(0, 300000);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);

    std::string path = "/kv/" + key;
    httplib::Result res;

    std::string json_body = "";
    if (!value_body.empty()) {
        json_body = "{\"key\":\"" + key + "\", \"value\":\"" + value_body + "\"}";
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    if (method == "GET") {
        res = cli.Get(path);
    } else if (method == "POST") {
        res = cli.Post(path, json_body, "application/json");
    } else if (method == "PUT") {
        res = cli.Put(path, json_body, "application/json");
    } else if (method == "DELETE") {
        res = cli.Delete(path);
    } else {
        std::cerr << "Error: Invalid HTTP method specified." << std::endl;
        return "Error: Invalid HTTP method specified.";
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double latency_ms = static_cast<double>(duration.count()) / 1000.0;

    std::cout << "Request Latency: " << std::fixed << std::setprecision(3) << latency_ms << " ms" << std::endl;

    if (res) {
        std::cout << "HTTP Status: " << res->status << std::endl;
        return res->body;
    } else {
        auto err = res.error();
        std::cerr << "Network/Client Error: " << httplib::to_string(err) << std::endl;
        return "Error: " + httplib::to_string(err);
    }
}

int main() {
    std::cout << "Interactive KV Client" << std::endl;
    std::cout << "Server target: " << SERVER_HOST << ":" << SERVER_PORT << std::endl;

    std::string command;
    std::string key, value;
    
    while (true) {
        std::cout << "\nEnter command (add, get, update, delete, exit): ";
        std::cin >> command;

        if (command == "exit") {
            break;
        } else if (command == "get") { // sent to get
            std::cout << "Enter key: ";
            std::cin >> key;
            std::string response_body = send_http_request("GET", key);
            std::cout << "Server Response Body:\n" << response_body << std::endl;
        } else if (command == "add") { // sent to post
            std::cout << "Enter key to add: ";
            std::cin >> key;
            std::cout << "Enter value: ";
            std::cin.ignore(); // if this not used getline reads the newline char from prev cin and ends early
            std::getline(std::cin, value);

            std::string response_body = send_http_request("POST", key, value);
            std::cout << "Server Response Body:\n" << response_body << std::endl;
        } else if (command == "update") { // sent to put
            std::cout << "Enter key to update: ";
            std::cin >> key;
            std::cout << "Enter new value: ";
            std::cin.ignore();
            std::getline(std::cin, value);

            std::string response_body = send_http_request("PUT", key, value);
            std::cout << "Server Response Body:\n" << response_body << std::endl;
        } else if (command == "delete") {
            std::cout << "Enter key to delete: ";
            std::cin >> key;
            std::string response_body = send_http_request("DELETE", key);
            std::cout << "Server Response Body:\n" << response_body << std::endl;
        } else {
            std::cout << "Invalid command. Please use 'add', 'get', 'update', 'delete', or 'exit'." << std::endl;
        }
    }

    std::cout << "Exiting client." << std::endl;
    return 0;
}