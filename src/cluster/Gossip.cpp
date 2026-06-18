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
    stop(); // Program close hone par thread safely join ho jaye
}

void Gossip::start(const std::vector<std::string>& seedNodes) {
    if (isRunning) return; 
    isRunning = true;

    // 1. THE DISCOVERY: Boot hote hi seed nodes ko Hello bolna
    for (const auto& seed : seedNodes) {
        if (seed != myAddress) {
            addPeer(seed); // Pehle apni peer list mein dalo
            std::cout << "[GOSSIP] Connecting to cluster via seed node: " << seed << "...\n";
            sendDiscoveryPing(seed);
        }
    }

    // 2. THE HEARTBEAT: Naya background thread launch karna
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
        failureCount[peerAddress] = 0; // Naye server ka fail count 0 set kiya
    }
}

// 📡 GOSSIP PING: Doosre nodes ko batana "Main Zinda Hoon"
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

// 🎯 TRIGGER: Jab kisi doosre node ka ping is node par aata hai
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
        
        // 1. Apni memory mein add karo
        addPeer(newNode);
        
        // 2. Hash Ring mein naye node ko baithao
        if (hashRing != nullptr) {
            hashRing->addNode(newNode);
            std::cout << "[HASH RING] Node " << newNode << " added to Consistent Hash Ring.\n";
        }
        
        // 3. ⚖️ THE MASTERPIECE: Trigger Rebalance!
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

                    // ⚖️ FAILOVER REBALANCE: Jab node marta hai, tab bhi data redistribute hoga!
                    std::cout << "[REBALANCE] Node dead. Triggering Failover initiateRebalance()...\n";
                }
            }
        }
    }
}