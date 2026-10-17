#pragma once
#include <string>
#include "../../external/httplib.h"

// Forward declarations of cluster modules
class StorageEngine;
class ConsistentHash;
class WAL;
class Gossip;
class Replicator;

class Router {
private:
    std::string myAddress; // Node address (e.g., "127.0.0.1:8080")
    httplib::Server server;
    
    // Component pointers
    StorageEngine* storage;
    ConsistentHash* hashRing;
    WAL* wal;
    Gossip* gossip;
    Replicator* replicator;

    // Internal route initialization
    void setupRoutes();

public:
    Router(const std::string& address, StorageEngine* se, ConsistentHash* ch, 
           WAL* w, Gossip* g, Replicator* r);

    // Starts HTTP server on the given port
    void start(int port);

    // Initiates data rebalancing when a new node joins the cluster
    void initiateRebalance(const std::string& newNodeAddress);
};