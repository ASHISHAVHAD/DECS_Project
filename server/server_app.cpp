#include "server_app.h"
#include "config.h"
#include "cache.h"
#include "database.h"
#include "logger.h"

#include <mysql_driver.h>

// default constructor
ServerApp::ServerApp() {

}

void ServerApp::init(int num_threads) {

    server_threads = num_threads;

    log_message("Initializing ServerApp...");

    // Initialize Database Connection Pool
    // We create as many DB connections as there are worker threads to minimize waiting
    log_message("Initializing MySQL connection pool with " + std::to_string(50) + " connections...");
    db_init(50);

    log_message("Configuring httplib server with a thread pool of size " + std::to_string(server_threads));
    svr.new_task_queue = [this] {
        return new httplib::ThreadPool(server_threads);
    };

    // GET /kv/{key}
    svr.Get("/kv/([^/]+)", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        std::string log_msg_prefix = "GET /kv/" + key + " from " + req.remote_addr + ":" + std::to_string(req.remote_port);

        std::string value;
        std::string source_str;
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            value = cache_get(key); // checking in cache
        }

        if (!value.empty()) { // found in cache
            res.status = 200;
            source_str = "cache";
            res.set_content("{\"key\":\"" + key + "\", \"value\":\"" + value + "\", \"source\":\"cache\"}", "application/json");
        } else {
            // cache miss, goto database
            value = db_read(key);
            if (!value.empty()) { // found in database
                res.status = 200;
                source_str = "database (cache miss)";
                res.set_content("{\"key\":\"" + key + "\", \"value\":\"" + value + "\", \"source\":\"database\"}", "application/json");
                {
                    std::lock_guard<std::mutex> lock(cache_mutex); 
                    cache_put(key, value); // update cache
                }
            } else { //not found in database
                res.status = 404;
                source_str = "not found";
                res.set_content("{\"error\":\"Key not found\"}", "application/json");
            }
        }
        log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Source: " + source_str);
    });

    // POST /kv/{key}
    svr.Post("/kv/([^/]+)", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        std::string log_msg_prefix = "POST /kv/" + key + " from " + req.remote_addr + ":" + std::to_string(req.remote_port);

        std::string value_from_body = extract_value_from_json(req.body);
        if (value_from_body.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Missing value in request body or invalid JSON format\"}", "application/json");
            log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: Bad Request (missing value)");
            return;
        }

        if (db_create(key, value_from_body)) {
            res.status = 201; 
            res.set_content("{\"message\":\"Key-value pair created\"}", "application/json");
            {
                //std::lock_guard<std::mutex> lock(cache_mutex);
                //cache_put(key, value_from_body); 
            }
            log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Action: Created (DB+Cache)");
        } else {
            if (db_key_exists(key)) {
                res.status = 409;
                res.set_content("{\"error\":\"Key already exists. Use PUT to update.\"}", "application/json");
                log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: Conflict (Key exists)");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"Failed to store in database\"}", "application/json");
                log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: DB write failed");
            }
        }
    });

    // PUT /kv/{key}
    svr.Put("/kv/([^/]+)", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        std::string log_msg_prefix = "PUT /kv/" + key + " from " + req.remote_addr + ":" + std::to_string(req.remote_port);

        std::string value_from_body = extract_value_from_json(req.body);
        if (value_from_body.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Missing value in request body or invalid JSON format\"}", "application/json");
            log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: Bad Request (missing value)");
            return;
        }

        if (db_update(key, value_from_body)) {
            res.status = 200;
            res.set_content("{\"message\":\"Key-value pair updated\"}", "application/json");
            {
                std::lock_guard<std::mutex> lock(cache_mutex);
                cache_put(key, value_from_body); 
            }
            log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Action: Updated (DB+Cache)");
        } else {
            if (!db_key_exists(key)) {
                res.status = 404; 
                res.set_content("{\"error\":\"Key not found. Use POST to create.\"}", "application/json");
                log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: Not Found (Key missing)");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"Failed to update in database\"}", "application/json");
                log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: DB update failed");
            }
        }
    });

    // DELETE /kv/{key}
    svr.Delete("/kv/([^/]+)", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        std::string log_msg_prefix = "DELETE /kv/" + key + " from " + req.remote_addr + ":" + std::to_string(req.remote_port);

        if (db_delete(key)) {
            res.status = 200;
            res.set_content("{\"message\":\"Key-value pair deleted\"}", "application/json");
            {
                std::lock_guard<std::mutex> lock(cache_mutex);
                cache_delete(key); 
            }
            log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Action: Deleted (DB+Cache)");
        } else {
            if (!db_key_exists(key)) {
                 res.status = 200;
                 res.set_content("{\"error\":\"Key not found\"}", "application/json");
                 log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: Not Found (Key missing)");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"Failed to delete key from database\"}", "application/json");
                log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: DB delete failed");
            }
        }
    });
}

void ServerApp::run() {
    log_message("Server starting with " + std::to_string(server_threads) + " worker threads.");
    log_message("Listening on 0.0.0.0:" + std::to_string(SERVER_PORT));

    if (!svr.listen("0.0.0.0", SERVER_PORT)) {
        log_message("ERROR: Server failed to start or encountered an error.");
        exit(1);
    }
}

std::string ServerApp::extract_value_from_json(const std::string& json_body) {
    std::string value_from_body;
    size_t value_start = json_body.find("\"value\":\"");
    if (value_start != std::string::npos) {
        value_start += 9;
        size_t value_end = json_body.find("\"", value_start);
        if (value_end != std::string::npos) {
            value_from_body = json_body.substr(value_start, value_end - value_start);
        }
    }
    return value_from_body;
}