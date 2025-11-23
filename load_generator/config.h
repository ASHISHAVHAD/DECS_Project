#ifndef LOAD_GENERATOR_CONFIG_H
#define LOAD_GENERATOR_CONFIG_H

#include <string>

// -- Server Connection Configuration --
// Use 127.0.0.1 for local testing.
// If testing across a network, change this to the Server's LAN IP.
const std::string SERVER_HOST = "127.0.0.1";
const int SERVER_PORT = 8080;

// --- Workload Types Enum ---
enum WorkloadType {
    PUT_ALL,      // 0: Write-only (High DB contention)
    GET_ALL,      // 1: Read-only, uniform distribution (High Cache Miss)
    GET_POPULAR,  // 2: Read-only, Zipfian-like distribution (High Cache Hit)
    GET_PUT       // 3: Mixed Read/Write (Realistic)
};

#endif // LOAD_GENERATOR_CONFIG_H