#include "../../include/cluster/Gossip.hpp"
#include "../../include/core/ConsistentHash.hpp"
#include "../../external/httplib.h" // 'cpp-httplib' library HTTP calls ke liye
#include <chrono>
#include <iostream>
#include <algorithm> // std::find, std::remove ke liye

Gossip::Gossip(ConsistentHash* ring) : hashRing(ring), isRunning(false) {}

Gossip::~Gossip() {
    stop(); // Program close hone par thread safely join ho jaye
}

void Gossip::start() {
    if (isRunning) return; // Agar pehle se chal raha hai toh dobara start na kare
    
    isRunning = true;
    // Naya background thread launch karna
    heartbeatThread = std::thread(&Gossip::runHeartbeat, this);
    std::cout << "[Gossip] Heartbeat thread started." << std::endl;
}

void Gossip::stop() {
    if (isRunning) {
        isRunning = false; // Loop ko stop signal diya
        
        // Main thread ko wait karwayenge jab tak background thread apna kaam khatam karke properly close na ho jaye
        if (heartbeatThread.joinable()) {
            heartbeatThread.join();
        }
        std::cout << "[Gossip] Heartbeat thread stopped." << std::endl;
    }
}

void Gossip::addPeer(const std::string& peerAddress) {
    std::lock_guard<std::mutex> lock(peers_lock);
    
    // Agar list mein nahi hai toh hi add karein
    if (std::find(peers.begin(), peers.end(), peerAddress) == peers.end()) {
        peers.push_back(peerAddress);
        failureCount[peerAddress] = 0; // Naye server ka fail count 0 set kiya
    }
}

void Gossip::runHeartbeat() {
    // Background loop jo hamesha chalega jab tak isRunning true hai
    while (isRunning) {
        // Har 2 second ka gap (Heartbeat interval)
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Lock lagakar currently known peers ki list copy kar lo, 
        // taaki ping karte waqt lock jaldi free ho jaye aur main thread block na ho.
        std::vector<std::string> currentPeers;
        {
            std::lock_guard<std::mutex> lock(peers_lock);
            currentPeers = peers;
        }

        for (const auto& peer : currentPeers) {
            // BULLETPROOF CLIENT SETUP (Host aur Port ko alag karke bhejna)
            size_t colonPos = peer.find(':');
            std::string host = peer.substr(0, colonPos);
            int port = std::stoi(peer.substr(colonPos + 1));

            httplib::Client cli(host, port);
            
            // Timeout explicitly set karein (1 second, 0 microseconds)
            cli.set_connection_timeout(1, 0); 
            cli.set_read_timeout(1, 0);

            // DEBUG LOG: Yeh line har 2 second mein print hogi!
            // std::cout << "[Gossip] Pinging peer -> " << peer << "..." << std::endl;

            auto res = cli.Get("/ping");

            bool isAlive = (res && res->status == 200);

            // Ab status update karne ke liye lock wapas lagana padega
            std::lock_guard<std::mutex> lock(peers_lock);
            
            if (isAlive) {
                // Server ne reply kiya, fail count wapas 0 kar do
                failureCount[peer] = 0; 
                // std::cout << "[Gossip] " << peer << " is ALIVE." << std::endl; // Optional debug
            } else {
                // Server ne reply nahi kiya ya error aayi
                failureCount[peer]++;
                std::cerr << "[Gossip] Missed heartbeat for " << peer 
                          << " (Failures: " << failureCount[peer] << "/3)" << std::endl;

                // JD's Condition: 3 lagatar failure par server DEAD
                if (failureCount[peer] >= 3) {
                    std::cerr << "[Gossip] Node " << peer << " is DEAD! Evicting from cluster." << std::endl;
                    
                    if (hashRing) {
                        hashRing->removeNode(peer);
                    }
                    // List se hatana aur track se delete karna
                    peers.erase(std::remove(peers.begin(), peers.end(), peer), peers.end());
                    failureCount.erase(peer);
                }
            }
        }
    }
}