#pragma once
#include <string>

class Replicator {
public:
    Replicator() = default;

    // Synchronously forwards write to replica node; returns true on ACK
    bool forwardToReplica(const std::string& key, const std::string& value, const std::string& targetNode, int ttl = 0);

    // Forwards delete request to replica node
    void forwardDelete(const std::string& key, const std::string& targetNode);
};