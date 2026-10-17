#include <iostream>
#include <string>
#include <cstdlib>
#include <functional>
#include <vector>

#include "../include/core/StorageEngine.hpp"
#include "../include/core/ConsistentHash.hpp"
#include "../include/core/WAL.hpp"
#include "../include/cluster/Gossip.hpp"
#include "../include/cluster/Replicator.hpp"
#include "../include/network/Router.hpp"
#include "../include/core/Config.hpp"

ClusterConfig global_config;

int main(int argc, char* argv[]) {
    // 1. Validate command line arguments
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <PORT> [PEER1_IP:PORT] [PEER2_IP:PORT]..." << std::endl;
        std::cerr << "Example: ./kv_server 8080 127.0.0.1:8081 127.0.0.1:8082" << std::endl;
        return 1;
    }

    // Configure port and server address
    int port = std::stoi(argv[1]);
    std::string myAddress = "127.0.0.1:" + std::to_string(port);

    std::cout << "Starting KV-Store Node at " << myAddress << "..." << std::endl;

    // 2. Initialize in-memory storage engine (Capacity: 10,000 keys)
    StorageEngine storage(10000);

    // 3. Initialize Write-Ahead Logging (WAL) and crash recovery
    std::string walFileName = "data/wal_" + std::to_string(port) + ".log";
    WAL wal(walFileName);
    
    // Replay log from disk to recover memory state
    wal.recover(storage);

    // 4. Initialize Consistent Hash Ring (100 virtual nodes per physical node)
    ConsistentHash hashRing(100);
    hashRing.addNode(myAddress);

    // Add peer nodes to hash ring and populate seed nodes for discovery
    std::vector<std::string> seedNodes;
    for (int i = 2; i < argc; ++i) {
        std::string peerAddress = argv[i];
        hashRing.addNode(peerAddress);
        seedNodes.push_back(peerAddress);
    }

    // 5. Initialize Gossip Protocol for failure detection and discovery
    Gossip gossip(myAddress, &hashRing);
    gossip.start(seedNodes); 

    // 6. Initialize Replicator
    Replicator replicator;

    // 7. Initialize Router (API Gateway)
    Router router(myAddress, &storage, &hashRing, &wal, &gossip, &replicator);

    // 8. Start HTTP server (blocks main thread to handle incoming requests)
    std::cout << "Node is ready and listening for requests!" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;
    router.start(port);

    // Stop gossip thread gracefully on shutdown
    gossip.stop();

    return 0;
}