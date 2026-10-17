#include "../../include/core/ConsistentHash.hpp"
#include <functional>
#include <algorithm>

ConsistentHash::ConsistentHash(int v_nodes) : virtual_nodes(v_nodes) {}

// 64-bit FNV-1a Hash Function
size_t ConsistentHash::computeHash(const std::string& key) {
    size_t hash = 14695981039346656037ULL; // FNV offset basis
    for (char c : key) {
        hash ^= static_cast<size_t>(c);
        hash *= 1099511628211ULL;          // FNV prime
    }
    return hash;
}

// Adds a physical node to the hash ring with virtual nodes
void ConsistentHash::addNode(const std::string& nodeAddress) {
    std::unique_lock<std::shared_mutex> lock(ring_lock);

    for (int i = 0; i < virtual_nodes; ++i) {
        std::string virtual_node_name = nodeAddress + "#" + std::to_string(i);
        size_t hash_val = computeHash(virtual_node_name);
        hashRing[hash_val] = nodeAddress;
    }
}

// Removes a physical node and its virtual nodes from the ring
void ConsistentHash::removeNode(const std::string& nodeAddress) {
    std::unique_lock<std::shared_mutex> lock(ring_lock);

    for (int i = 0; i < virtual_nodes; ++i) {
        std::string virtual_node_name = nodeAddress + "#" + std::to_string(i);
        size_t hash_val = computeHash(virtual_node_name);
        hashRing.erase(hash_val);
    }
}

// Finds the owner node responsible for a given key
std::string ConsistentHash::getOwnerNode(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(ring_lock);

    if (hashRing.empty()) {
        return "";
    }

    size_t hash_val = computeHash(key);
    
    // Find the first virtual node with hash >= key hash (clockwise ring traversal)
    auto it = hashRing.upper_bound(hash_val);

    // Wrap around to the start of the ring if past the end
    if (it == hashRing.end()) {
        it = hashRing.begin();
    }

    return it->second;
}

// Returns unique physical nodes currently active on the ring
std::vector<std::string> ConsistentHash::getUniquePhysicalNodes() {
    std::shared_lock<std::shared_mutex> lock(ring_lock);
    std::vector<std::string> unique_nodes;
    
    for (const auto& pair : hashRing) {
        if (std::find(unique_nodes.begin(), unique_nodes.end(), pair.second) == unique_nodes.end()) {
            unique_nodes.push_back(pair.second);
        }
    }
    return unique_nodes;
}