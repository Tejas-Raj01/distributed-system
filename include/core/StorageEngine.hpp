#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <mutex>
#include <optional>

class StorageEngine {
private:
    size_t capacity; // Database ki maximum limit
    
    // Core Data Structures
    std::unordered_map<std::string, std::string> dataStore;
    std::list<std::string> lruQueue;
    std::unordered_map<std::string, std::list<std::string>::iterator> lruMap;
    
    // Reader-Writer Lock
    mutable std::shared_mutex rw_lock;

    // Helper function LRU eviction ke liye
    void evictOldest();

public:
    // Constructor jisme memory limit set hogi
    StorageEngine(size_t max_capacity);

    // Core Functions
    void put(const std::string& key, const std::string& value);
    
    // std::optional use kar rahe hain taaki agar data na mile toh null return kar sakein
    std::optional<std::string> get(const std::string& key);
    
    bool remove(const std::string& key);
};