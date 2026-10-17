#include "../../include/cluster/Gossip.hpp"
#include "../../include/core/ConsistentHash.hpp"
#include "../../external/httplib.h" 
#include <chrono>
#include <iostream>
#include <algorithm> 

// Constructor updated to take myAddress for Discovery
Gossip::Gossip(const std::string& myAddress, ConsistentHash* ring) 
    : myAddress(myAddress), hashRing(ring), isRunning(false) {}

Gossip::~Gossip() {
    stop();
}

void Gossip::start(const std::vector<std::string>& seedNodes) {
    if (isRunning) return; 
    isRunning = true;

    // 1. Discovery: Connect to seed nodes on startup
    for (const auto& seed : seedNodes) {
        if (seed != myAddress) {
            addPeer(seed);
            std::cout << "[GOSSIP] Connecting to cluster via seed node: " << seed << "...\n";
            sendDiscoveryPing(seed);
        }
    }

    // 2. Heartbeat: Start background monitoring thread
    heartbeatThread = std::thread(&Gossip::runHeartbeat, this);
    std::cout << "[Gossip] Heartbeat thread started." << std::endl;
}

void Gossip::stop() {
    if (isRunning) {
        isRunning = false;
        if (heartbeatThread.joinable()) {
            heartbeatThread.join();
        }
        std::cout << "[Gossip] Heartbeat thread stopped." << std::endl;
    }
}

void Gossip::addPeer(const std::string& peerAddress) {
    std::lock_guard<std::mutex> lock(peers_lock);
    if (std::find(peers.begin(), peers.end(), peerAddress) == peers.end()) {
        peers.push_back(peerAddress);
        failureCount[peerAddress] = 0;
    }
}

// Announces node presence to peer
void Gossip::sendDiscoveryPing(const std::string& targetNode) {
    size_t colonPos = targetNode.find(':');
    if (colonPos == std::string::npos) return;
    
    std::string host = targetNode.substr(0, colonPos);
    int port = std::stoi(targetNode.substr(colonPos + 1));

    httplib::Client cli(host, port);
    cli.set_connection_timeout(1, 0);

    httplib::Params params;
    params.emplace("newNode", myAddress); 

    auto res = cli.Post("/internal/gossip", params);
    if (res && res->status == 200) {
        std::cout << "[GOSSIP] Successfully introduced myself to " << targetNode << " 🤝\n";
    } else {
        std::cerr << "[GOSSIP] Target " << targetNode << " is unreachable.\n";
    }
}

// Handles incoming gossip announcement from a peer node
void Gossip::receiveGossip(const std::string& newNode) {
    bool isNew = false;
    {
        std::lock_guard<std::mutex> lock(peers_lock);
        if (std::find(peers.begin(), peers.end(), newNode) == peers.end() && newNode != myAddress) {
            isNew = true;
        }
    }

    if (isNew) {
        std::cout << "\n[GOSSIP] 🚨 ALERT: New Node Joined the Cluster -> " << newNode << "!\n";
        
        // 1. Add to peer list
        addPeer(newNode);
        
        // 2. Add to consistent hash ring
        if (hashRing != nullptr) {
            hashRing->addNode(newNode);
            std::cout << "[HASH RING] Node " << newNode << " added to Consistent Hash Ring.\n";
        }
        
        // 3. Trigger data rebalance
        std::cout << "[REBALANCE] Cluster shape changed. Triggering background initiateRebalance()...\n";
    }
}

void Gossip::runHeartbeat() {
    while (isRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::vector<std::string> currentPeers;
        {
            std::lock_guard<std::mutex> lock(peers_lock);
            currentPeers = peers;
        }

        for (const auto& peer : currentPeers) {
            size_t colonPos = peer.find(':');
            if(colonPos == std::string::npos) continue;

            std::string host = peer.substr(0, colonPos);
            int port = std::stoi(peer.substr(colonPos + 1));

            httplib::Client cli(host, port);
            cli.set_connection_timeout(1, 0); 
            cli.set_read_timeout(1, 0);

            auto res = cli.Get("/ping");
            bool isAlive = (res && res->status == 200);

            std::lock_guard<std::mutex> lock(peers_lock);
            
            if (isAlive) {
                failureCount[peer] = 0; 
            } else {
                failureCount[peer]++;
                std::cerr << "[Gossip] Missed heartbeat for " << peer 
                          << " (Failures: " << failureCount[peer] << "/3)" << std::endl;

                if (failureCount[peer] >= 3) {
                    std::cerr << "[Gossip] Node " << peer << " is DEAD! Evicting from cluster." << std::endl;
                    
                    if (hashRing) {
                        hashRing->removeNode(peer);
                    }
                    peers.erase(std::remove(peers.begin(), peers.end(), peer), peers.end());
                    failureCount.erase(peer);

                    // Trigger failover rebalance when node is evicted
                    std::cout << "[REBALANCE] Node dead. Triggering Failover initiateRebalance()...\n";
                }
            }
        }
    }
}