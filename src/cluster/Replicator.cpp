#include "../../include/cluster/Replicator.hpp"
#include "../../external/httplib.h"
#include <iostream>
#include <string>

// Synchronous replication call for Strict Quorum (W)
bool Replicator::forwardToReplica(const std::string& key, const std::string& value, const std::string& targetNode, int ttl) {
    if (targetNode.empty()) return false;

    size_t colonPos = targetNode.find(':');
    std::string host = targetNode.substr(0, colonPos);
    int port = std::stoi(targetNode.substr(colonPos + 1));

    httplib::Client cli(host, port);
    
    // 2-second timeout for synchronous quorum call
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(2, 0);

    httplib::Params params;
    params.emplace("key", key);
    params.emplace("value", value);
    if (ttl > 0) {
        params.emplace("ttl", std::to_string(ttl));
    }

    auto res = cli.Post("/internal/replicate", params);

    if (res && res->status == 200) {
        std::cout << "[Quorum] ACK received from Replica -> " << targetNode << std::endl;
        return true;
    } else {
        std::cerr << "[Quorum] FAILED: No response from Replica -> " << targetNode << std::endl;
        return false;
    }
}

// Forward Delete request to replica
void Replicator::forwardDelete(const std::string& key, const std::string& targetNode) {
    if (targetNode.empty()) return;

    size_t colonPos = targetNode.find(':');
    std::string host = targetNode.substr(0, colonPos);
    int port = std::stoi(targetNode.substr(colonPos + 1));

    httplib::Client cli(host, port);
    cli.set_connection_timeout(1, 0);
    cli.set_read_timeout(1, 0);

    auto res = cli.Delete("/internal/delete?key=" + key);
    
    if (res && res->status == 200) {
        std::cout << "[Replicator] Successfully forwarded delete for key: " << key << " to " << targetNode << std::endl;
    } else {
        std::cerr << "[Replicator] Failed to forward delete for key: " << key << " to " << targetNode << std::endl;
    }
}