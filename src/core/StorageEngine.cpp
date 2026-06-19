#include "../../include/core/StorageEngine.hpp"
#include <chrono>
#include <iostream>
#include <vector>

// Helper: Current epoch time nikalne ke liye (in seconds)
long long getCurrentTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Constructor: Thread ko start karo
StorageEngine::StorageEngine(size_t max_capacity) : capacity(max_capacity), is_running(true) {
    cleanup_thread = std::thread(&StorageEngine::cleanupTask, this);
}

// Destructor: Thread ko safely stop karo
StorageEngine::~StorageEngine() {
    is_running = false;
    if (cleanup_thread.joinable()) {
        cleanup_thread.join();
    }
}

// THE GARBAGE COLLECTOR
void StorageEngine::cleanupTask() {
    while (is_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Har 1 second mein utho
        
        // Write lock lagao kyunki hum data delete kar sakte hain
        std::unique_lock<std::shared_mutex> lock(rw_lock);
        long long now = getCurrentTime();

        // Poore map ko scan karo
        for (auto it = dataStore.begin(); it != dataStore.end(); ) {
            // Agar expiry_time 0 se bada hai aur current time se chota ho gaya hai (Expire ho gaya)
            if (it->second.expiry_time > 0 && it->second.expiry_time < now) {
                // LRU list aur Map dono se udao
                std::cout << "[TTL] Key expired and evicted: " << it->first << std::endl;
                lruQueue.erase(lruMap[it->first]);
                lruMap.erase(it->first);
                it = dataStore.erase(it); // Safe deletion while iterating
            } else {
                ++it;
            }
        }
    }
}

// Helper Function: Sabse purani key ko delete karne ke liye
void StorageEngine::evictOldest() {
    if (lruQueue.empty()) return;
    
    std::string oldest_key = lruQueue.back();
    lruQueue.pop_back();       
    lruMap.erase(oldest_key);  
    dataStore.erase(oldest_key); 
}

// PUT Operation mein ttl_seconds add kiya aur Record struct use kiya
void StorageEngine::put(const std::string& key, const std::string& value, int ttl_seconds) {
    std::unique_lock<std::shared_mutex> lock(rw_lock); // Write Lock

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

// GET Operation mein Lazy eviction check kiya aur Record se .value nikala
std::optional<std::string> StorageEngine::get(const std::string& key) {
    // Unique lock zaroori hai kyunki hum LRU queue modify karte hain (push_front)
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    if (dataStore.find(key) == dataStore.end()) {
        return std::nullopt; 
    }

    // Lazy Eviction: Agar background thread se pehle get call ho gaya, check expire time
    if (dataStore[key].expiry_time > 0 && dataStore[key].expiry_time < getCurrentTime()) {
        lruQueue.erase(lruMap[key]);
        lruMap.erase(key);
        dataStore.erase(key);
        return std::nullopt; // Expire ho chuka hai
    }

    lruQueue.erase(lruMap[key]);
    lruQueue.push_front(key);
    lruMap[key] = lruQueue.begin();

    return dataStore[key].value;
}

// REMOVE Operation
bool StorageEngine::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(rw_lock); // Write Lock

    if (dataStore.find(key) == dataStore.end()) {
        return false; 
    }

    lruQueue.erase(lruMap[key]);
    lruMap.erase(key);
    dataStore.erase(key);
    
    return true;
}

// GET ALL DATA (Rebalancing ke liye)
std::vector<std::pair<std::string, std::string>> StorageEngine::getAllData() {
    // Shared lock lagayenge taaki jab hum data read kar rahe hon, 
    // toh baaki log bhi read kar sakein bina block hue
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    
    std::vector<std::pair<std::string, std::string>> all_data;
    for (const auto& item : dataStore) {
        // item.first = key, item.second.value = data
        all_data.push_back({item.first, item.second.value}); 
    }
    return all_data;
}

// ==========================================
// 🧹 CLEAR ALL DATA (For UI Reset Button)
// ==========================================
void StorageEngine::clearAll() {
    // Thread-safety ke liye pehle memory ko lock karenge (Unique Lock kyunki write operation hai)
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    
    // C++ map ka in-built function jo poori dictionary khali kar deta hai
    dataStore.clear(); 
}