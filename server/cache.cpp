#include "cache.h"
#include "config.h"

std::list<CacheEntry> lru_list;
std::map<std::string, std::list<CacheEntry>::iterator> lru_map; //using a map for O(1) access to cache
std::mutex cache_mutex;

// LRU - Least recently used cache

// Mutexes will have to be obtained by caller before calling these functions

void cache_put(const std::string& key, const std::string& value) {
    auto it = lru_map.find(key);
    if (it != lru_map.end()) {
        // if key exists, update value and move to front to mark as lru
        it->second->value = value;
        lru_list.splice(lru_list.begin(), lru_list, it->second); //moves node pointed by it->second to the front of lru_list 
    } else {
        // if key is new
        if (lru_list.size() == MAX_CACHE_SIZE) {
            // if cache is full evict oldest key i.e. back of the list
            lru_map.erase(lru_list.back().key);
            lru_list.pop_back();
        }
        // add new entry to front of lru_list
        lru_list.push_front({key, value});
        lru_map[key] = lru_list.begin();
    }
}

std::string cache_get(const std::string& key) {
    auto it = lru_map.find(key);
    if (it != lru_map.end()) {
        // if found, move the key to front as it is most recently used
        lru_list.splice(lru_list.begin(), lru_list, it->second);
        return it->second->value;
    }
    return ""; // if not found return empty string
}

// this function is necessary for cache coherence
// if a key-value pair is deleted from database it must be deleted from cache to maintain consistency
void cache_delete(const std::string& key) {
    auto it = lru_map.find(key);
    if (it != lru_map.end()) {
        lru_list.erase(it->second);
        lru_map.erase(it);
    }
}