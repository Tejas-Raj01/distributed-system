#include "../../include/core/StorageEngine.hpp"
#include <chrono>
#include <iostream>

// Helper: Current epoch time nikalne ke liye (in seconds)
long long getCurrentTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Constructor
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
    
    // Queue ke last mein sabse purani key hoti hai
    std::string oldest_key = lruQueue.back();
    lruQueue.pop_back();       // List se delete kiya
    lruMap.erase(oldest_key);  // Iterator map se delete kiya
    dataStore.erase(oldest_key); // Asli data store se delete kiya
}

// PUT Operation
void StorageEngine::put(const std::string& key, const std::string& value) {
    // Exclusive Write-Lock: Koi aur read/write nahi kar sakta
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    // Agar key pehle se exist karti hai, toh value update karo aur usko naya (recent) banao
    if (dataStore.find(key) != dataStore.end()) {
        dataStore[key] = value;
        lruQueue.erase(lruMap[key]); // Purani position se hatao
        lruQueue.push_front(key);    // Sabse aage lagao
        lruMap[key] = lruQueue.begin(); // Nayi position save karo
        return;
    }

    // Nayi key insert karne se pehle check karo ki memory full toh nahi
    if (dataStore.size() >= capacity) {
        evictOldest();
    }

    // Naya data insert karo aur usko recently used mark karo
    dataStore[key] = value;
    lruQueue.push_front(key);
    lruMap[key] = lruQueue.begin();
}

// GET Operation
std::optional<std::string> StorageEngine::get(const std::string& key) {
    // NOTE: Yahan 'shared_lock' nahi laga sakte kyunki hum LRU list ko modify kar rahe hain.
    // Isliye Exclusive Lock use karna padega data safety ke liye.
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    if (dataStore.find(key) == dataStore.end()) {
        return std::nullopt; // Key nahi mili
    }

    // Data mil gaya, ab isko LRU list mein sabse aage (top) laana hai
    lruQueue.erase(lruMap[key]);
    lruQueue.push_front(key);
    lruMap[key] = lruQueue.begin();

    return dataStore[key];
}

// REMOVE Operation
bool StorageEngine::remove(const std::string& key) {
    // Exclusive Write-Lock
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    if (dataStore.find(key) == dataStore.end()) {
        return false; // Delete karne ke liye kuch nahi mila
    }

    // Data aur uske saare LRU trackers delete kar do
    lruQueue.erase(lruMap[key]);
    lruMap.erase(key);
    dataStore.erase(key);
    
    return true;
}
    
