#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

// Forward declaration: Taaki Gossip class ko pata ho ki ConsistentHash exist karta hai
class ConsistentHash; 

class Gossip {
private:
    std::string myAddress;         // 🆕 NAYA: Is node ki khud ki identity (e.g., "127.0.0.1:8086")
    
    std::atomic<bool> isRunning;   // Thread-safe boolean (loop control)
    std::thread heartbeatThread;   // Background OS thread
    
    std::vector<std::string> peers; // Doosre servers ki list (e.g., "127.0.0.1:8081")
    std::mutex peers_lock;          // Peer list ko safely modify karne ke liye lock
    
    // Track karega ki lagatar kitni baar ping fail hua hai
    std::unordered_map<std::string, int> failureCount;
    
    // Hash Ring ka pointer taaki dead node ko ring se nikal sakein
    ConsistentHash* hashRing;

    // Background thread ka main function
    void runHeartbeat();

public:
    // 🔄 UPDATE: Constructor mein ab myAddress bhi pass hoga
    Gossip(const std::string& myAddress, ConsistentHash* ring);
    
    // Destructor (safely thread ko band karega)
    ~Gossip();

    // 🔄 UPDATE: start function ab seed nodes accept karega
    void start(const std::vector<std::string>& seedNodes);
    
    void stop();
    void addPeer(const std::string& peerAddress);

    // 🆕 NAYA: Cluster Discovery & Rebalance Triggers
    void sendDiscoveryPing(const std::string& targetNode);
    void receiveGossip(const std::string& newNode);
};