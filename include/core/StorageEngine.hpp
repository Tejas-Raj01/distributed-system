#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <thread>   // NAYA: Background thread ke liye
#include <atomic>   // NAYA: Thread ko safely stop karne ke liye
#include <vector>

struct Record {
    std::string value;
    long long expiry_time; 
};

class StorageEngine {
private:
    size_t capacity; // Database ki maximum limit
    
    // Core Data Structures
    std::unordered_map<std::string, Record> dataStore;
    std::list<std::string> lruQueue;
    std::unordered_map<std::string, std::list<std::string>::iterator> lruMap;
    

    // Reader-Writer Lock
    mutable std::shared_mutex rw_lock;
 
    // NAYA: Background Garbage Collector
    std::atomic<bool> is_running;
    std::thread cleanup_thread;

    // Helper function LRU eviction ke liye
    void evictOldest();
    void cleanupTask(); // Thread jo purane data ko delete karega

public:
    // NAYA: Data rebalancing ke liye saara data nikalne ka function
    std::vector<std::pair<std::string, std::string>> getAllData();
    
    // Constructor jisme memory limit set hogi
    StorageEngine(size_t max_capacity);
    ~StorageEngine();

    // Core Functions
    void put(const std::string& key, const std::string& value, int ttl_seconds = 0);
    
    // std::optional use kar rahe hain taaki agar data na mile toh null return kar sakein
    std::optional<std::string> get(const std::string& key);
    
    bool remove(const std::string& key);
};