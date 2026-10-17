#include "../../include/core/StorageEngine.hpp"
#include <chrono>
#include <iostream>
#include <vector>

// Helper: Returns current epoch time in seconds
long long getCurrentTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

StorageEngine::StorageEngine(size_t max_capacity) : capacity(max_capacity), is_running(true) {
    cleanup_thread = std::thread(&StorageEngine::cleanupTask, this);
}

StorageEngine::~StorageEngine() {
    is_running = false;
    if (cleanup_thread.joinable()) {
        cleanup_thread.join();
    }
}

// Background TTL garbage collector
void StorageEngine::cleanupTask() {
    while (is_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::unique_lock<std::shared_mutex> lock(rw_lock);
        long long now = getCurrentTime();

        for (auto it = dataStore.begin(); it != dataStore.end(); ) {
            // Evict expired keys
            if (it->second.expiry_time > 0 && it->second.expiry_time < now) {
                std::cout << "[TTL] Key expired and evicted: " << it->first << std::endl;
                lruQueue.erase(lruMap[it->first]);
                lruMap.erase(it->first);
                it = dataStore.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// Evicts the least recently used item
void StorageEngine::evictOldest() {
    if (lruQueue.empty()) return;
    
    std::string oldest_key = lruQueue.back();
    lruQueue.pop_back();       
    lruMap.erase(oldest_key);  
    dataStore.erase(oldest_key); 
}

// Inserts or updates key-value pair with optional TTL
void StorageEngine::put(const std::string& key, const std::string& value, int ttl_seconds) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    long long expiry = 0;
    if (ttl_seconds > 0) {
        expiry = getCurrentTime() + ttl_seconds;
    }

    if (dataStore.find(key) != dataStore.end()) {
        dataStore[key] = {value, expiry}; 
        lruQueue.erase(lruMap[key]); 
        lruQueue.push_front(key);    
        lruMap[key] = lruQueue.begin(); 
        return;
    }

    if (dataStore.size() >= capacity) {
        evictOldest();
    }

    dataStore[key] = {value, expiry}; 
    lruQueue.push_front(key);
    lruMap[key] = lruQueue.begin();
}

// Retrieves value for a key with lazy eviction on TTL expiry
std::optional<std::string> StorageEngine::get(const std::string& key) {
    // Unique lock required because LRU queue order is updated on read
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    if (dataStore.find(key) == dataStore.end()) {
        return std::nullopt; 
    }

    // Lazy Eviction: remove key if expired before background cleanup
    if (dataStore[key].expiry_time > 0 && dataStore[key].expiry_time < getCurrentTime()) {
        lruQueue.erase(lruMap[key]);
        lruMap.erase(key);
        dataStore.erase(key);
        return std::nullopt;
    }

    lruQueue.erase(lruMap[key]);
    lruQueue.push_front(key);
    lruMap[key] = lruQueue.begin();

    return dataStore[key].value;
}

// Removes a key from the storage engine
bool StorageEngine::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    if (dataStore.find(key) == dataStore.end()) {
        return false; 
    }

    lruQueue.erase(lruMap[key]);
    lruMap.erase(key);
    dataStore.erase(key);
    
    return true;
}

// Retrieves all key-value pairs for cluster rebalancing
std::vector<std::pair<std::string, std::string>> StorageEngine::getAllData() {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    
    std::vector<std::pair<std::string, std::string>> all_data;
    for (const auto& item : dataStore) {
        all_data.push_back({item.first, item.second.value}); 
    }
    return all_data;
}

// Clears all stored data and LRU structures
void StorageEngine::clearAll() {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    
    dataStore.clear(); 
    lruQueue.clear();
    lruMap.clear();
}