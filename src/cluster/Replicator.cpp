#include "../../include/cluster/Replicator.hpp"
#include "../../external/httplib.h" // HTTP requests ke liye
#include <thread>    // Async threading ke liye
#include <iostream>

bool Replicator::forwardToReplica(const std::string& key, const std::string& value, const std::string& targetNode, int ttl) {
    if (targetNode.empty()) return false;

    size_t colonPos = targetNode.find(':');
    std::string host = targetNode.substr(0, colonPos);
    int port = std::stoi(targetNode.substr(colonPos + 1));

    httplib::Client cli(host, port);
    
    // Max 2 sec wait karega, uske baad timeout error dega
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(2, 0);

    httplib::Params params;
    params.emplace("key", key);
    params.emplace("value", value);
    if (ttl > 0) {
        params.emplace("ttl", std::to_string(ttl));
    }

    // NAYA: Hum /put ki jagah /internal/put par bhejenge
    auto res = cli.Post("/internal/put", params);

    if (res && res->status == 200) {
        std::cout << "[Quorum] ACK received from Replica -> " << targetNode << std::endl;
        return true; // Backup Success
    } else {
        std::cerr << "[Quorum] FAILED: No response from Replica -> " << targetNode << std::endl;
        return false; // Backup Failed
    }
}

void Replicator::forwardDelete(const std::string& key, const std::string& targetNode) {
    // Bulletproof host aur port split logic
    size_t colonPos = targetNode.find(':');
    std::string host = targetNode.substr(0, colonPos);
    int port = std::stoi(targetNode.substr(colonPos + 1));

    httplib::Client cli(host, port);
    cli.set_connection_timeout(1, 0);
    cli.set_read_timeout(1, 0);

    // Internal delete route par request bhejenge
    auto res = cli.Delete("/internal/delete?key=" + key);
    
    if (res && res->status == 200) {
        std::cout << "[Replicator] Successfully forwarded delete for key: " << key << " to " << targetNode << std::endl;
    } else {
        std::cerr << "[Replicator] Failed to forward delete for key: " << key << " to " << targetNode << std::endl;
    }
}