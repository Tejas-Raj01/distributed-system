#include <iostream>
#include <string>
#include <cstdlib> // std::stoi ke liye
#include <functional>
#include <vector>  // 🚀 NAYA: std::vector use karne ke liye include kiya

// Saare custom modules include karein
#include "../include/core/StorageEngine.hpp"
#include "../include/core/ConsistentHash.hpp"
#include "../include/core/WAL.hpp"
#include "../include/cluster/Gossip.hpp"
#include "../include/cluster/Replicator.hpp"
#include "../include/network/Router.hpp"
#include "../include/core/Config.hpp"

ClusterConfig global_config;

int main(int argc, char* argv[]) {
    // 1. Command line arguments check karein
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <PORT> [PEER1_IP:PORT] [PEER2_IP:PORT]..." << std::endl;
        std::cerr << "Example: ./kv_server 8080 127.0.0.1:8081 127.0.0.1:8082" << std::endl;
        return 1;
    }

    // Port aur Server Address setup
    int port = std::stoi(argv[1]);
    std::string myAddress = "127.0.0.1:" + std::to_string(port);

    std::cout << "Starting KV-Store Node at " << myAddress << "..." << std::endl;

    // 2. Storage Engine (RAM) initialize karein (Limit: 10,000 keys)
    StorageEngine storage(10000);

    // 3. Write-Ahead Logging (WAL) setup aur Crash Recovery
    // Har server ki apni alag log file hogi, e.g., "wal_8080.log"
    std::string walFileName = "data/wal_" + std::to_string(port) + ".log";
    WAL wal(walFileName);
    
    // Server start hote hi pehle disk se data RAM mein wapas laao
    wal.recover(storage);

    // 4. Consistent Hashing (Router Ring) setup
    ConsistentHash hashRing(100); // 100 virtual nodes per physical node
    hashRing.addNode(myAddress); // Apne aap ko ring mein daalo

    // 🚀 THE FIX: Command line se peers nikal kar unko seedNodes vector mein daalna
    std::vector<std::string> seedNodes;
    for (int i = 2; i < argc; ++i) {
        std::string peerAddress = argv[i];
        hashRing.addNode(peerAddress);
        seedNodes.push_back(peerAddress); // Naye discovery engine ke liye list banayi
    }

    // 5. Gossip Protocol (Failure Detection & Discovery)
    // 🚀 THE FIX: Constructor mein ab 'myAddress' pass ho raha hai
    Gossip gossip(myAddress, &hashRing);
    
    // 🚀 THE FIX: start() function mein ab 'seedNodes' pass ho rahe hain taaki ping ja sake
    gossip.start(seedNodes); 

    // 6. Replicator setup
    Replicator replicator;

    // 7. Router (API Gateway) setup
    Router router(myAddress, &storage, &hashRing, &wal, &gossip, &replicator);

    // 8. Server ko Live karein (Yeh main thread ko block kar dega aur HTTP requests listen karega)
    std::cout << "Node is ready and listening for requests!" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;
    router.start(port);

    // Jab server band hoga (Ctrl+C), toh Gossip thread ko safely stop karo
    gossip.stop();

    return 0;
}