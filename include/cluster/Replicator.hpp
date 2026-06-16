#pragma once
#include <string>

class Replicator {
public:
    // Constructor (Isme kisi special state ki zarurat nahi hai)
    Replicator() = default;

    // Background mein doosre server ko data bhejne ka function
    void forwardToReplica(const std::string& key, const std::string& value, const std::string& nextNode);
};