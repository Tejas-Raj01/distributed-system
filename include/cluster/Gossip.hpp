#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

// Forward declaration for ConsistentHash
class ConsistentHash; 

class Gossip {
private:
    std::string myAddress;          // Current node address (e.g., "127.0.0.1:8086")
    
    std::atomic<bool> isRunning;    // Thread-safe boolean (loop control)
    std::thread heartbeatThread;    // Background heartbeat thread
    
    std::vector<std::string> peers; // Peer node addresses (e.g., "127.0.0.1:8081")
    std::mutex peers_lock;          // Mutex protecting peers list
    
    // Tracks consecutive heartbeat failure count per peer
    std::unordered_map<std::string, int> failureCount;
    
    // Pointer to hash ring for topology updates
    ConsistentHash* hashRing;

    // Background thread execution loop
    void runHeartbeat();

public:
    Gossip(const std::string& myAddress, ConsistentHash* ring);
    ~Gossip();

    void start(const std::vector<std::string>& seedNodes);
    void stop();
    void addPeer(const std::string& peerAddress);

    // Cluster discovery & rebalance triggers
    void sendDiscoveryPing(const std::string& targetNode);
    void receiveGossip(const std::string& newNode);
};