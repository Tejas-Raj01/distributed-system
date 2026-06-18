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
    
    // 1. Handle Pre-flight OPTIONS requests (Browser real request se pehle yeh bhejta hai)
    server.Options("(.*)", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 200;
    });

    // 1. GOSSIP HEARTBEAT API (/ping)
    // C++ Backend Code (Router setup mein add karein)
server.Get("/ping", [](const httplib::Request& req, httplib::Response& res) {
    // 1. CORS Headers zaroori hain!
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    
    // 2. Success message bhejein
    res.set_content("{\"status\":\"ok\"}", "application/json");
});

// Agar browser OPTIONS preflight request bhejta hai (CORS check ke liye)
server.Options("/ping", [](const httplib::Request& req, httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
});

    // 1.5 REPLICATION API
    server.Post("/internal/replicate", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        wal->appendLog("PUT", key, value);
        storage->put(key, value);
        res.status = 200;
    });

    // 1. PUBLIC PUT API (With CORS & JSON)
    server.Post("/put", [this](const httplib::Request& req, httplib::Response& res) {
        // MANUALLY ADDED CORS HEADER HERE
        res.set_header("Access-Control-Allow-Origin", "*");

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
            wal->appendLog("PUT", key, value);
            storage->put(key, value, ttl);
            int acks = 1; 
            
            bool replicaAck = false;
            if (myAddress == "127.0.0.1:8080") {
                replicaAck = replicator->forwardToReplica(key, value, "127.0.0.1:8082", ttl);
            } else if (myAddress == "127.0.0.1:8082") {
                replicaAck = replicator->forwardToReplica(key, value, "127.0.0.1:8080", ttl);
            }

            if (replicaAck) acks++;

            if (acks >= 2) {
                res.status = 200;
                res.set_content(R"({"status": "success"})", "application/json");
            } else {
                storage->remove(key); 
                wal->appendLog("DELETE", key, ""); 
                res.status = 503;
                res.set_content(R"({"error": "Quorum Failed"})", "application/json");
            }
        } else {
            httplib::Client cli("http://" + ownerNode);
            httplib::Params params;
            params.emplace("key", key);
            params.emplace("value", value);
            if (ttl > 0) params.emplace("ttl", std::to_string(ttl));
            
            auto proxyRes = cli.Post("/put", params);
            if (proxyRes) {
                res.status = proxyRes->status;
                res.set_content(proxyRes->body, "application/json"); 
            } else {
                res.status = 503;
                res.set_content(R"({"error": "Owner node is unreachable"})", "application/json");
            }
        }
    });

    // 2. INTERNAL PUT API
    server.Post("/internal/put", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        int ttl = req.has_param("ttl") ? std::stoi(req.get_param_value("ttl")) : 0;
        wal->appendLog("PUT", key, value);
        storage->put(key, value, ttl);
        res.status = 200;
        res.set_content("Internal Backup Saved", "text/plain");
    });

    // 3. GET API (/get)
    server.Get("/get", [this](const httplib::Request& req, httplib::Response& res) {
        // MANUALLY ADDED CORS HEADER HERE
        res.set_header("Access-Control-Allow-Origin", "*");

        if (!req.has_param("key")) {
            res.status = 400;
            res.set_content("Missing 'key'", "text/plain");
            return;
        }

        std::string key = req.get_param_value("key");
        std::string ownerNode = hashRing->getOwnerNode(key);

        if (ownerNode == myAddress) {
            auto result = storage->get(key);
            if (result.has_value()) {
                res.status = 200;
                res.set_content(result.value(), "text/plain");
            } else {
                res.status = 404;
                res.set_content("Key Not Found", "text/plain");
            }
        } else {
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

    // PUBLIC DELETE API
    server.Delete("/delete", [this](const httplib::Request& req, httplib::Response& res) {
        // MANUALLY ADDED CORS HEADER HERE
        res.set_header("Access-Control-Allow-Origin", "*");

        std::string key = req.get_param_value("key");
        if (key.empty()) {
            res.status = 400;
            res.set_content("Missing key parameter", "text/plain");
            return;
        }

        bool isDeleted = storage->remove(key);
        wal->appendLog("DELETE", key, "");

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

    // INTERNAL DELETE API
    server.Delete("/internal/delete", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        wal->appendLog("DELETE", key, "");
        storage->remove(key);
        res.status = 200;
        res.set_content("Backup Deleted", "text/plain");
    });

    // ADMIN API: Data Rebalancing
    server.Get("/admin/rebalance", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        auto allData = storage->getAllData();
        int transferCount = 0;

        for (const auto& item : allData) {
            std::string key = item.first;
            std::string value = item.second;
            std::string currentOwner = hashRing->getOwnerNode(key);

            if (currentOwner != myAddress) {
                size_t colonPos = currentOwner.find(':');
                std::string host = currentOwner.substr(0, colonPos);
                int port = std::stoi(currentOwner.substr(colonPos + 1));

                httplib::Client cli(host, port);
                cli.set_connection_timeout(2, 0);
                
                httplib::Params params;
                params.emplace("key", key);
                params.emplace("value", value);
                
                auto transferRes = cli.Post("/internal/put", params);
                
                if (transferRes && transferRes->status == 200) {
                    storage->remove(key);
                    wal->appendLog("DELETE", key, "");
                    transferCount++;
                }
            }
        }
        std::string jsonResponse = R"({"status": "rebalanced", "keys_transferred": )" + std::to_string(transferCount) + R"(})";
        res.set_content(jsonResponse, "application/json");
    });
}

// Server ko port par listen karwana
void Router::start(int port) {
    std::cout << "[Router] API Gateway starting on port " << port << "..." << std::endl;
    // "0.0.0.0" ka matlab hai yeh server external network aur localhost dono se connect ho sakta hai
    server.listen("0.0.0.0", port);
}