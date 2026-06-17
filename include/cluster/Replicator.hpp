#pragma once
#include <string>

class Replicator {
public:
    // Constructor (Isme kisi special state ki zarurat nahi hai)
    Replicator() = default;

    // NAYA: Ab yeh bool return karega aur ttl bhi accept karega
    bool forwardToReplica(const std::string& key, const std::string& value, const std::string& targetNode, int ttl = 0);

    void forwardDelete(const std::string& key, const std::string& targetNode);
};