#pragma once
#include <string>
#include <map>
#include <shared_mutex>
#include <mutex>
#include <vector>

class ConsistentHash {
private:
    // Virtual Nodes ki ginti (Ek physical server ke ring mein kitne replicas honge)
    // Yeh data ko evenly distribute karne mein madad karta hai.
    int virtual_nodes;

    // The Ring: Key (Hash Value) -> Value (Physical Node Address/Port)
    // std::map internally sorted rehta hai, jo ring ka effect deta hai.
    std::map<size_t, std::string> hashRing;
    
    // Read-Write Lock
    mutable std::shared_mutex ring_lock;

public:
    // Constructor: Default 3 virtual nodes per server set kiye hain
    ConsistentHash(int v_nodes = 3);

    // Hash function jo string ko bade integer mein badlega
    size_t computeHash(const std::string& key);

    // Core Router Functions
    void addNode(const std::string& nodeAddress);
    void removeNode(const std::string& nodeAddress);
    std::string getOwnerNode(const std::string& key);

    // Zinda (Active) physical nodes ki list nikalne ke liye
    std::vector<std::string> getUniquePhysicalNodes();
};