// load_generator/main.cpp
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <mutex>
#include <iomanip> // For std::fixed, std::setprecision

// --- cpp-httplib header ---
#include "../httplib.h"

// Load generator's own config header
#include "config.h"

// --- Global Statistics ---
std::atomic<long long> total_requests_completed(0);
std::atomic<long long> total_successful_requests(0);
std::atomic<long long> total_failed_requests(0);
std::atomic<long long> total_response_time_ns(0);
std::atomic<long long> total_successful_get_requests(0);
std::atomic<long long> total_cache_hits(0);
std::mutex cout_mutex;

const int GET_ALL_PREPOP_COUNT = 20000;

// --- Helper Functions ---
std::string generate_random_string(size_t length) {
    const std::string CHARACTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string s(length, ' ');
    thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);
    for (size_t i = 0; i < length; ++i) {
        s[i] = CHARACTERS[distribution(generator)];
    }
    return s;
}

// Function that each client thread will run
void client_thread_task(int thread_id, int num_total_clients, WorkloadType workload,
                        long long test_duration_ms,
                        int key_range_start, int key_range_end,
                        const std::vector<std::string>& popular_keys)
{
    httplib::Client cli(SERVER_HOST, SERVER_PORT);
    cli.set_connection_timeout(0, 5000000); // 500 ms
    cli.set_read_timeout(5, 0); // 5 seconds
    cli.set_write_timeout(5, 0); // 5 seconds

    std::mt19937 generator(std::random_device{}() + thread_id);
    std::uniform_int_distribution<int> key_dist(key_range_start, key_range_end);
    std::uniform_int_distribution<int> op_dist(0, 99);
    std::uniform_int_distribution<int> popular_key_idx_dist(0, popular_keys.empty() ? 0 : popular_keys.size() - 1);
    std::uniform_int_distribution<int> existing_key_dist(0, GET_ALL_PREPOP_COUNT - 1);
    std::uniform_int_distribution<int> non_existing_key_dist(GET_ALL_PREPOP_COUNT, key_range_end);

    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() >= test_duration_ms) {
            break;
        }

        std::string method;
        std::string key;
        std::string json_body;

        switch (workload) {
            case PUT_ALL: {
                method = "POST";
                key = std::to_string(num_total_clients) + "_key_t" + std::to_string(thread_id) + "_" + std::to_string(key_dist(generator));
                json_body = "{\"key\":\"" + key + "\", \"value\":\"" + generate_random_string(32) + "\"}";
                break;
            }
            case GET_ALL: {
                method = "GET";
                //if (op_dist(generator) < 50) {
                    key = "key_" + std::to_string(existing_key_dist(generator));
                //} else {
                   //key = "key_" + std::to_string(non_existing_key_dist(generator));
                //}
                break;
            }
            case GET_POPULAR: {
                method = "GET";
                key = !popular_keys.empty() ? popular_keys[popular_key_idx_dist(generator)] : "popular_key_0";
                break;
            }
            case GET_PUT: {
                int op_rand = op_dist(generator);
                if (op_rand < 60) {
                    method = "GET";
                    key = "key_" + std::to_string(key_dist(generator));
                } else if (op_rand < 90) {
                    method = (op_rand < 75) ? "POST" : "PUT";
                    key = "key_" + std::to_string(key_dist(generator));
                    json_body = "{\"key\":\"" + key + "\", \"value\":\"" + generate_random_string(32) + "\"}";
                } else {
                    method = "DELETE";
                    key = "key_" + std::to_string(key_dist(generator));
                }
                break;
            }
        }

        std::string path = "/kv/" + key;
        httplib::Result res;
        auto request_start_time = std::chrono::high_resolution_clock::now();

        if (method == "GET") { res = cli.Get(path); }
        else if (method == "POST") { res = cli.Post(path, json_body, "application/json"); }
        else if (method == "PUT") { res = cli.Put(path, json_body, "application/json"); }
        else if (method == "DELETE") { res = cli.Delete(path); }

        auto request_end_time = std::chrono::high_resolution_clock::now();
        
        // --- BUG FIX HERE ---
        // The latency is the duration between the start and end of the request.
        long long response_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(request_end_time - request_start_time).count();

        total_requests_completed++;

        if (res) {
            if (res->status >= 200 && res->status < 500) {
                total_successful_requests++;
                total_response_time_ns += response_time_ns;

                if (method == "GET" && res->status == 200) {
                    total_successful_get_requests++;
                    if (res->body.find("\"source\":\"cache\"") != std::string::npos) {
                        total_cache_hits++;
                    }
                }
            } else {
                total_failed_requests++;
            }
        } else {
            total_failed_requests++;
            auto err = res.error();
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Thread " << thread_id << " Network Error: " << httplib::to_string(err) << std::endl;
            }
        }
    }
}

// The 'main' function remains exactly the same as the last version.
// No changes are needed below this point.
int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_clients> <test_duration_seconds> <workload_type> [key_range_start] [key_range_end] [num_popular_keys]" << std::endl;
        std::cerr << "Workload Types: 0=PUT_ALL, 1=GET_ALL, 2=GET_POPULAR, 3=GET_PUT" << std::endl;
        return 1;
    }

    int num_clients = std::stoi(argv[1]);
    int test_duration_seconds = std::stoi(argv[2]);
    WorkloadType workload = static_cast<WorkloadType>(std::stoi(argv[3]));
    long long test_duration_ms = static_cast<long long>(test_duration_seconds) * 1000;

    int key_range_start = 0;
    int key_range_end = 2000000;
    if (argc >= 6) {
        key_range_start = std::stoi(argv[4]);
        key_range_end = std::stoi(argv[5]);
    }

    std::vector<std::string> popular_keys;
    httplib::Client cli_init(SERVER_HOST, SERVER_PORT);

    if (workload == GET_ALL) {
        /*std::cout << "Pre-populating " << GET_ALL_PREPOP_COUNT << " keys for GET_ALL workload..." << std::endl;
        for (int i = 0; i < GET_ALL_PREPOP_COUNT; ++i) {
            std::string p_key = "key_" + std::to_string(i);
            std::string init_json_body = "{\"key\":\"" + p_key + "\", \"value\":\"" + generate_random_string(32) + "\"}";
            cli_init.Post("/kv/" + p_key, init_json_body, "application/json");
        }
        std::cout << "Pre-population complete." << std::endl;*/
    } else if (workload == GET_POPULAR) {
        int num_popular_keys_to_generate = (argc >= 7) ? std::stoi(argv[6]) : 100;
        for (int i = 0; i < num_popular_keys_to_generate; ++i) {
            popular_keys.push_back("popular_key_" + std::to_string(i));
        }
        std::cout << "Generated " << popular_keys.size() << " popular keys..." << std::endl;
        /*std::cout << "Pre-populating popular keys in the database..." << std::endl;
        for(const auto& p_key : popular_keys) {
            std::string init_json_body = "{\"key\":\"" + p_key + "\", \"value\":\"" + generate_random_string(32) + "\"}";
            cli_init.Post("/kv/" + p_key, init_json_body, "application/json");
        }
        std::cout << "Pre-population complete." << std::endl;*/
    }

    std::cout << "\nStarting load test..." << std::endl;
    std::vector<std::thread> threads;
    auto overall_start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back(client_thread_task, i, num_clients, workload,
                             test_duration_ms, key_range_start, key_range_end,
                             std::cref(popular_keys));
    }

    for (auto& t : threads) { t.join(); }

    auto overall_end_time = std::chrono::high_resolution_clock::now();
    double actual_test_duration_seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(overall_end_time - overall_start_time).count() / 1e9;

    long long successful_requests = total_successful_requests.load();
    long long failed_requests = total_failed_requests.load();
    long long completed_requests = total_requests_completed.load();
    long long total_response_time_sum_ns = total_response_time_ns.load();
    long long successful_gets = total_successful_get_requests.load();
    long long cache_hits = total_cache_hits.load();

    std::cout << "\n--- Load Test Results ---" << std::endl;
    std::cout << "Test Duration (actual):    " << std::fixed << std::setprecision(3) << actual_test_duration_seconds << " seconds" << std::endl;
    std::cout << "Number of Clients:         " << num_clients << std::endl;
    std::cout << "Total Requests Attempted:  " << completed_requests << std::endl;
    std::cout << "Successful Requests:       " << successful_requests << " (includes 4xx client errors)" << std::endl;
    std::cout << "Failed Requests:           " << failed_requests << " (includes 5xx server errors)" << std::endl;

    if (successful_requests > 0) {
        double average_throughput = static_cast<double>(successful_requests) / actual_test_duration_seconds;
        double average_response_time_ms = (successful_requests > 0) ? (static_cast<double>(total_response_time_sum_ns) / (successful_requests * 1e6)) : 0.0;
        double cache_hit_rate = (successful_gets > 0) ? (static_cast<double>(cache_hits) / successful_gets * 100.0) : 0.0;

        std::cout << "---------------------------------" << std::endl;
        std::cout << "Average Throughput:        " << std::fixed << std::setprecision(2) << average_throughput << " req/s" << std::endl;
        std::cout << "Average Response Time:     " << std::fixed << std::setprecision(3) << average_response_time_ms << " ms" << std::endl;
        std::cout << "Cache Hit Rate (for GETs): " << std::fixed << std::setprecision(2) << cache_hit_rate << " % (" << cache_hits << "/" << successful_gets << ")" << std::endl;
        std::cout << "---------------------------------" << std::endl;
    } else {
        std::cout << "No successful requests to calculate average metrics." << std::endl;
    }

    return 0;
}