#include "../../include/core/StorageEngine.hpp"

// Constructor
StorageEngine::StorageEngine(size_t max_capacity) : capacity(max_capacity) {}

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
    
