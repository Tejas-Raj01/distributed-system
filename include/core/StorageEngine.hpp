#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <thread>
#include <atomic>
#include <vector>

struct Record {
    std::string value;
    long long expiry_time; 
};

class StorageEngine {
private:
    size_t capacity; // Maximum key capacity
    
    // Core Data Structures
    std::unordered_map<std::string, Record> dataStore;
    std::list<std::string> lruQueue;
    std::unordered_map<std::string, std::list<std::string>::iterator> lruMap;

    // Reader-Writer Lock (Thread Safety)
    mutable std::shared_mutex rw_lock;
 
    // Background Garbage Collector
    std::atomic<bool> is_running;
    std::thread cleanup_thread;

    // Helper functions for eviction and cleanup
    void evictOldest();
    void cleanupTask(); // Background thread worker for expired keys

public:
    StorageEngine(size_t max_capacity);
    ~StorageEngine();

    // Core storage operations
    void put(const std::string& key, const std::string& value, int ttl_seconds = 0);
    std::optional<std::string> get(const std::string& key);
    bool remove(const std::string& key);

    void clearAll();

    // Retrieves all key-value pairs for cluster rebalancing
    std::vector<std::pair<std::string, std::string>> getAllData();
};