#include "../../include/network/Router.hpp"
#include "../../include/core/StorageEngine.hpp"
#include "../../include/core/ConsistentHash.hpp"
#include "../../include/core/WAL.hpp"
#include "../../include/cluster/Gossip.hpp"
#include "../../include/cluster/Replicator.hpp"
#include <iostream>

// Constructor: Saare pointers ko initialize karna aur routes setup karna
Router::Router(const std::string& address, StorageEngine* se, ConsistentHash* ch, 
               WAL* w, Gossip* g, Replicator* r)
    : myAddress(address), storage(se), hashRing(ch), wal(w), gossip(g), replicator(r) {
    
    setupRoutes();
}

// APIs aur Unka Logic map karna
void Router::setupRoutes() {
    
    // 1. GOSSIP HEARTBEAT API (/ping)
    server.Get("/ping", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 200;
        res.set_content("PONG", "text/plain");
    });

    // 1.5 REPLICATION API (Backup receive karne ke liye)
    server.Post("/internal/replicate", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        
        // Backup ko chupchaap apni RAM aur Log mein save kar lo
        wal->appendLog("PUT", key, value);
        storage->put(key, value);
        
        std::cout << "[Replicator] Backup received & saved for key: " << key << std::endl;
        res.status = 200;
    });

    // 2. PUT API (/put) - Client se data receive karna
    server.Post("/put", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("key") || !req.has_param("value")) {
            res.status = 400;
            res.set_content("Missing 'key' or 'value'", "text/plain");
            return;
        }

        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");

        // Hash Ring se pucho is key ka maalik (owner) kaun hai
        std::string ownerNode = hashRing->getOwnerNode(key);

        if (ownerNode == myAddress) {
            // SCENARIO A: Yeh node hi is data ka maalik hai
            std::cout << "[Router] Storing data locally for key: " << key << std::endl;
            
            // Pehle WAL mein likho (Crash safety)
            wal->appendLog("PUT", key, value);
            
            // Phir RAM (Storage Engine) mein save karo
            storage->put(key, value);

            if (myAddress == "127.0.0.1:8080") {
                replicator->forwardToReplica(key, value, "127.0.0.1:8082");
            }

            // Background mein next node (Replica) ko data bhej do
            // (Note: Real world mein next node dhoondhne ka ek function hashRing mein hota hai)
            // replicator->forwardToReplica(key, value, nextNodeAddress); 

            res.status = 200;
            res.set_content("Data Saved Successfully!", "text/plain");
        } 
        else {
            // SCENARIO B: Maalik koi aur hai, request wahan PROXY (forward) karo
            std::cout << "[Router] Proxying PUT request to owner: " << ownerNode << std::endl;
            
            httplib::Client cli("http://" + ownerNode);
            httplib::Params params;
            params.emplace("key", key);
            params.emplace("value", value);
            
            auto proxyRes = cli.Post("/put", params);
            
            if (proxyRes) {
                res.status = proxyRes->status;
                res.set_content(proxyRes->body, "text/plain");
            } else {
                res.status = 503;
                res.set_content("Owner node is unreachable", "text/plain");
            }
        }
    });

    // 3. GET API (/get) - Data read karna
    server.Get("/get", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("key")) {
            res.status = 400;
            res.set_content("Missing 'key'", "text/plain");
            return;
        }

        std::string key = req.get_param_value("key");
        std::string ownerNode = hashRing->getOwnerNode(key);

        if (ownerNode == myAddress) {
            // SCENARIO A: Mere paas hi data hai
            auto result = storage->get(key);
            
            if (result.has_value()) {
                res.status = 200;
                res.set_content(result.value(), "text/plain");
            } else {
                res.status = 404;
                res.set_content("Key Not Found", "text/plain");
            }
        } 
        else {
            // SCENARIO B: Data kisi aur ke paas hai, Request usko PROXY karo
            std::cout << "[Router] Proxying GET request to owner: " << ownerNode << std::endl;
            
            httplib::Client cli("http://" + ownerNode);
            auto proxyRes = cli.Get("/get?key=" + key);
            
            if (proxyRes) {
                res.status = proxyRes->status;
                res.set_content(proxyRes->body, "text/plain");
            } else {
                res.status = 503;
                res.set_content("Owner node is unreachable", "text/plain");
            }
        }
    });
}

// Server ko port par listen karwana
void Router::start(int port) {
    std::cout << "[Router] API Gateway starting on port " << port << "..." << std::endl;
    // "0.0.0.0" ka matlab hai yeh server external network aur localhost dono se connect ho sakta hai
    server.listen("0.0.0.0", port);
}