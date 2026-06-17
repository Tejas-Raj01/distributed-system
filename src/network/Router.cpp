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

    // 1. PUBLIC PUT API (With Strict Quorum W=2 & React-Friendly JSON Response)
    server.Post("/put", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("key") || !req.has_param("value")) {
            res.status = 400;
            res.set_content(R"({"error": "Missing key or value"})", "application/json");
            return;
        }

        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        int ttl = req.has_param("ttl") ? std::stoi(req.get_param_value("ttl")) : 0;

        std::string ownerNode = hashRing->getOwnerNode(key);

        if (ownerNode == myAddress) {
            std::cout << "\n[Quorum] Processing Write Request for Key: " << key << std::endl;
            
            // STEP 1: Local RAM aur WAL mein save karo
            wal->appendLog("PUT", key, value);
            storage->put(key, value, ttl);
            int acks = 1; 
            
            // STEP 2: Backup Node (Replica) ko data bhejo
            bool replicaAck = false;
            if (myAddress == "127.0.0.1:8080") {
                replicaAck = replicator->forwardToReplica(key, value, "127.0.0.1:8082", ttl);
            } else if (myAddress == "127.0.0.1:8082") {
                replicaAck = replicator->forwardToReplica(key, value, "127.0.0.1:8080", ttl);
            }

            if (replicaAck) {
                acks++;
            }

            // STEP 3: Quorum Logic (W=2 Check)
            if (acks >= 2) {
                res.status = 200;
                // FIX: React ke liye pure JSON response bheja
                res.set_content(R"({"status": "success"})", "application/json");
                std::cout << "[Quorum] SUCCESS: W=2 reached.\n" << std::endl;
            } else {
                // ROLLBACK: Agar backup fail hua toh local se bhi hata do
                std::cout << "[Quorum] ROLLBACK: Quorum failed. Reverting local write.\n" << std::endl;
                storage->remove(key); 
                wal->appendLog("DELETE", key, ""); 
                
                res.status = 503;
                // FIX: React ke liye pure JSON error format
                res.set_content(R"({"error": "Quorum Failed"})", "application/json");
            }
        } 
        else {
            // PROXY LOGIC (Baki code waise hi rahega, owner node khud JSON return karega)
            std::cout << "[Router] Proxying PUT request to owner: " << ownerNode << std::endl;
            httplib::Client cli("http://" + ownerNode);
            httplib::Params params;
            params.emplace("key", key);
            params.emplace("value", value);
            if (ttl > 0) params.emplace("ttl", std::to_string(ttl));
            
            auto proxyRes = cli.Post("/put", params);
            if (proxyRes) {
                res.status = proxyRes->status;
                res.set_content(proxyRes->body, "application/json"); // Content type updated
            } else {
                res.status = 503;
                res.set_content(R"({"error": "Owner node is unreachable"})", "application/json");
            }
        }
    });

    // 2. INTERNAL PUT API (For Replicas only - Bina Hash Ring / Quorum ke)
    server.Post("/internal/put", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        int ttl = req.has_param("ttl") ? std::stoi(req.get_param_value("ttl")) : 0;

        wal->appendLog("PUT", key, value);
        storage->put(key, value, ttl);

        res.status = 200;
        res.set_content("Internal Backup Saved", "text/plain");
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

    // 1. PUBLIC DELETE API (Client ke liye)
    server.Delete("/delete", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        if (key.empty()) {
            res.status = 400;
            res.set_content("Missing key parameter", "text/plain");
            return;
        }

        // Local RAM se data delete karo
        bool isDeleted = storage->remove(key);

        // WAL mein "DELETE" log karo taaki crash recovery ke baad data wapas na aa jaye
        wal->appendLog("DELETE", key, "");

        // Replicator ko call karke baaki nodes se safaya karo
        // Hardcoded replication mapping: 8080 ka backup 8082 hai, aur 8082 ka 8080
        if (myAddress == "127.0.0.1:8080") {
            replicator->forwardDelete(key, "127.0.0.1:8082");
        } else if (myAddress == "127.0.0.1:8082") {
            replicator->forwardDelete(key, "127.0.0.1:8080");
        }

        if (isDeleted) {
            res.status = 200;
            res.set_content("Key Deleted Successfully from cluster", "text/plain");
        } else {
            res.status = 404;
            res.set_content("Key Not Found locally, but delete forwarded", "text/plain");
        }
    });

    // 2. INTERNAL DELETE API (Replicator ke use ke liye)
    server.Delete("/internal/delete", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        
        // Backup node bhi apne WAL aur RAM dono se delete karega
        wal->appendLog("DELETE", key, "");
        storage->remove(key);

        std::cout << "[Replicator] Internal backup delete executed for key: " << key << std::endl;
        res.status = 200;
        res.set_content("Backup Deleted", "text/plain");
    });
}

// Server ko port par listen karwana
void Router::start(int port) {
    std::cout << "[Router] API Gateway starting on port " << port << "..." << std::endl;
    // "0.0.0.0" ka matlab hai yeh server external network aur localhost dono se connect ho sakta hai
    server.listen("0.0.0.0", port);
}