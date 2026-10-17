#pragma once
#include <string>
#include <map>
#include <shared_mutex>
#include <mutex>
#include <vector>

class ConsistentHash {
private:
    // Virtual nodes per physical node for even distribution
    int virtual_nodes;

    // The Hash Ring: Key (Hash Value) -> Physical Node Address
    // std::map provides sorted order for ring traversal
    std::map<size_t, std::string> hashRing;
    
    // Read-Write lock for thread safety
    mutable std::shared_mutex ring_lock;

public:
    ConsistentHash(int v_nodes = 3);

    // Computes 64-bit FNV-1a hash
    size_t computeHash(const std::string& key);

    // Core ring functions
    void addNode(const std::string& nodeAddress);
    void removeNode(const std::string& nodeAddress);
    std::string getOwnerNode(const std::string& key);

    // Retrieves list of unique active physical nodes
    std::vector<std::string> getUniquePhysicalNodes();
};