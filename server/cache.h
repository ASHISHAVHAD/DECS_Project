#ifndef SERVER_CACHE_H
#define SERVER_CACHE_H

#include <string>
#include <list>
#include <map>
#include <mutex>

// cache entry struct
struct CacheEntry {
    std::string key;
    std::string value;
};

extern std::list<CacheEntry> lru_list;
extern std::map<std::string, std::list<CacheEntry>::iterator> lru_map;
extern std::mutex cache_mutex;

// Cache implementation is LRU
void cache_put(const std::string& key, const std::string& value);
std::string cache_get(const std::string& key); // Returns empty string if not found
void cache_delete(const std::string& key);

#endif