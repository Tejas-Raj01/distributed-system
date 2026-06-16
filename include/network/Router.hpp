#pragma once
#include <string>
#include "../../external/httplib.h" // HTTP Server aur Client dono ke liye

// Baaki sabhi modules ki forward declarations
class StorageEngine;
class ConsistentHash;
class WAL;
class Gossip;
class Replicator;

class Router {
private:
    std::string myAddress; // Is server ka khud ka address (e.g., "127.0.0.1:8080")
    httplib::Server server; // Web server object
    
    // Sabhi components ke pointers
    StorageEngine* storage;
    ConsistentHash* hashRing;
    WAL* wal;
    Gossip* gossip;
    Replicator* replicator;

    // Routing setup karne ka internal function
    void setupRoutes();

public:
    // Constructor jisme hum saare engines pass karenge
    Router(const std::string& address, StorageEngine* se, ConsistentHash* ch, 
           WAL* w, Gossip* g, Replicator* r);

    // Server start karne ka function
    void start(int port);
};