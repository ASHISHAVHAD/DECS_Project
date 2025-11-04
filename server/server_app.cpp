#include "server_app.h"
#include "config.h"
#include "cache.h"
#include "database.h"
#include "logger.h"

#include <mysql_driver.h>

// default constructor
ServerApp::ServerApp() {

}

void ServerApp::init() {

    log_message("Initializing ServerApp...");

    // initializing mysql driver
    try {
        sql::mysql::get_mysql_driver_instance();
        log_message("MySQL driver instance obtained successfully.");
    } catch (sql::SQLException &e) {
        log_message("ERROR: Could not get MySQL driver instance: " + std::string(e.what()));
        exit(1);
    }

    // GET /kv/{key}
    svr.Get("/kv/([^/]+)", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];
        std::string log_msg_prefix = "GET /kv/" + key + " from " + req.remote_addr + ":" + std::to_string(req.remote_port);

        std::string value;
        std::string source_str;
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            value = cache_get(key); // checking in cache, returns empty string if not found in cache
        }

        if (!value.empty()) { // found in cache, hence cache hit
            res.status = 200;
            source_str = "cache";
            res.set_content("{\"key\":\"" + key + "\", \"value\":\"" + value + "\", \"source\":\"cache\"}", "application/json");
        } else {
            // Cache miss, goto database
            value = db_read(key);
            if (!value.empty()) { // found in database
                res.status = 200;
                source_str = "database (cache miss)";
                res.set_content("{\"key\":\"" + key + "\", \"value\":\"" + value + "\", \"source\":\"database\"}", "application/json");
                {
                    std::lock_guard<std::mutex> lock(cache_mutex); // locking cache for writing to cache
                    cache_put(key, value); // putting this key-value in cache as recently used value
                }
            } else { //not found in database
                res.status = 404;
                source_str = "not found";
                res.set_content("{\"error\":\"Key not found\"}", "application/json");
            }
        }
        log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Source: " + source_str);
    });

    // POST /kv/{key} -> for creating and adding the key-value pair to database
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
            res.status = 201; // this status code is returned if a new resource is created at the server
            res.set_content("{\"message\":\"Key-value pair created\"}", "application/json");
            {
                std::lock_guard<std::mutex> lock(cache_mutex);
                cache_put(key, value_from_body); // add to cache as recently used
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

    // PUT /kv/{key} -> for updating the value corresponding to existing key
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
                cache_put(key, value_from_body); // update value in the cache also to maintain cache coherence with database
            }
            log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Action: Updated (DB+Cache)");
        } else {
            if (!db_key_exists(key)) {
                res.status = 404; // key not found
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
                cache_delete(key); // remove from cache also
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

    // error handler for requests that don't match any route
    svr.set_error_handler([&](const httplib::Request& req, httplib::Response& res) {
        std::string log_msg_prefix = "Unhandled request " + req.method + " " + req.path + " from " + req.remote_addr + ":" + std::to_string(req.remote_port);
        const char* fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
        char buf[BUFSIZ];
        snprintf(buf, sizeof(buf), fmt, res.status);
        res.set_content(buf, "text/html");
        log_message(log_msg_prefix + " -> Status: " + std::to_string(res.status) + ", Error: Unhandled Route");
    });
}

void ServerApp::run() {
    log_message("Server starting with " + std::to_string(std::thread::hardware_concurrency() - 1) + " worker threads.");
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