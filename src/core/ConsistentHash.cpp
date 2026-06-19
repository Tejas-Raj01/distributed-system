#include "../../include/core/ConsistentHash.hpp"
#include <functional> // std::hash ke liye
#include <algorithm>

// Constructor
ConsistentHash::ConsistentHash(int v_nodes) : virtual_nodes(v_nodes) {}

// Hashing Function
// THE FIX: Production-Grade FNV-1a Hash Function
size_t ConsistentHash::computeHash(const std::string& key) {
    size_t hash = 14695981039346656037ULL; // FNV offset basis
    for (char c : key) {
        hash ^= static_cast<size_t>(c);
        hash *= 1099511628211ULL;          // FNV prime
    }
    return hash;
}

// Naya server ring mein add karna
void ConsistentHash::addNode(const std::string& nodeAddress) {
    // Write Lock: Kyunki hum ring structure modify kar rahe hain
    std::unique_lock<std::shared_mutex> lock(ring_lock);

    for (int i = 0; i < virtual_nodes; ++i) {
        // Virtual node ka naam banate hain (e.g., "127.0.0.1:8080#0", "127.0.0.1:8080#1")
        std::string virtual_node_name = nodeAddress + "#" + std::to_string(i);
        size_t hash_val = computeHash(virtual_node_name);
        
        // Hash value ko map mein daal do, jo point karegi asli node address ko
        hashRing[hash_val] = nodeAddress;
    }
}

// Server ko ring se hatana (Agar crash ho jaye)
void ConsistentHash::removeNode(const std::string& nodeAddress) {
    // Write Lock
    std::unique_lock<std::shared_mutex> lock(ring_lock);

    for (int i = 0; i < virtual_nodes; ++i) {
        std::string virtual_node_name = nodeAddress + "#" + std::to_string(i);
        size_t hash_val = computeHash(virtual_node_name);
        
        // Ring se is virtual node ko delete karo
        hashRing.erase(hash_val);
    }
}

// Key ke liye sahi server dhoondhna
std::string ConsistentHash::getOwnerNode(const std::string& key) {
    // Read Lock: Yahan multiple threads ek sath aakar server dhoondh sakte hain
    std::shared_lock<std::shared_mutex> lock(ring_lock);

    if (hashRing.empty()) {
        return ""; // Agar ring mein koi server hi nahi hai
    }

    size_t hash_val = computeHash(key);
    
    // std::map ka upper_bound() exactly wahi karta hai jo hume chahiye.
    // Yeh us pehle element ka iterator dega jiski key 'hash_val' se badi hai. (Clockwise movement)
    auto it = hashRing.upper_bound(hash_val);

    // Agar hum ring ke end tak pahunch gaye, toh gol ghum kar wapas 0 degree (start) par aa jao
    if (it == hashRing.end()) {
        it = hashRing.begin();
    }

    // Physical node ka address return karo
    return it->second;
}

// NAYA: Backend se asli physical nodes ki list nikalna
std::vector<std::string> ConsistentHash::getUniquePhysicalNodes() {
    std::shared_lock<std::shared_mutex> lock(ring_lock);
    std::vector<std::string> unique_nodes;
    
    // Hash Ring mein loop chalakar unique addresses filter out karna
    for (const auto& pair : hashRing) {
        if (std::find(unique_nodes.begin(), unique_nodes.end(), pair.second) == unique_nodes.end()) {
            unique_nodes.push_back(pair.second);
        }
    }
    return unique_nodes;
}