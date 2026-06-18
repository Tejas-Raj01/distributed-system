#include "../../include/network/Router.hpp"
#include "../../include/core/StorageEngine.hpp"
#include "../../include/core/ConsistentHash.hpp"
#include "../../include/core/WAL.hpp"
#include "../../include/cluster/Gossip.hpp"
#include "../../include/cluster/Replicator.hpp"
#include "../../include/core/Config.hpp" 
#include <iostream>
#include <thread>
#include <chrono>
#include <regex> // 📦 NAYA: JSON array ko unpack karne ke liye

// Global ClusterConfig instance ko consume karne ke liye extern use kiya
extern ClusterConfig global_config;

// Constructor: Saare pointers ko initialize karna aur routes setup karna
Router::Router(const std::string& address, StorageEngine* se, ConsistentHash* ch, 
               WAL* w, Gossip* g, Replicator* r)
    : myAddress(address), storage(se), hashRing(ch), wal(w), gossip(g), replicator(r) {
    
    setupRoutes();
}

// APIs aur Unka Logic map karna
void Router::setupRoutes() {
    
    // 1. Handle Pre-flight OPTIONS requests (Browser ke CORS preflight check ke liye)
    server.Options("(.*)", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 200;
    });

    // 2. GOSSIP HEARTBEAT API (/ping)
    server.Get("/ping", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // 3. CONFIG SYNC ENDPOINT (UI -> Backend)
    server.Post("/admin/config", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        auto parse_json_param = [&](const std::string& key) {
            size_t pos = req.body.find("\"" + key + "\":");
            if (pos == std::string::npos) return -1;
            size_t start = pos + key.length() + 3;
            size_t end = req.body.find_first_of(",}", start);
            return std::stoi(req.body.substr(start, end - start));
        };

        int n = parse_json_param("N");
        int w = parse_json_param("W");
        int r = parse_json_param("R");

        global_config.update(n, w, r);
        std::cout << "[CONFIG] Cluster consensus rules updated via UI -> N:" << global_config.N 
                  << " W:" << global_config.W << " R:" << global_config.R << "\n";
        
        res.status = 200;
        res.set_content(R"({"status":"success"})", "application/json");
    });

    // ==========================================
    // 🗣️ 4. GOSSIP API: Node Discovery Endpoint (THE TRIGGER)
    // ==========================================
    server.Post("/internal/gossip", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("newNode")) {
            res.status = 400;
            return;
        }

        std::string newNode = req.get_param_value("newNode");
        std::cout << "\n[ROUTER] 📨 Received Gossip intro from new node: " << newNode << "\n";
        
        if (this->gossip != nullptr) {
            this->gossip->receiveGossip(newNode); // Hash Ring Update karega
        }

        // 🚀 MASTERSTROKE: Background thread mein data rebalance start taaki main thread block na ho!
        std::thread([this, newNode]() {
            this->initiateRebalance(newNode);
        }).detach();

        res.status = 200;
        res.set_content("Welcome to the cluster!", "text/plain");
    });

    // ==========================================
    // 🚚 5. REBALANCE RECEIVER: Naya Node Data Accept Karega
    // ==========================================
    server.Post("/internal/transfer", [this](const httplib::Request& req, httplib::Response& res) {
        std::cout << "\n[TRANSFER] 📦 Incoming bulk data transfer received...\n";
        
        // Regex magic: JSON format {"k":"key_name","v":"value_name"} se data nikalna
        std::regex e("\\{\"k\":\"(.*?)\",\"v\":\"(.*?)\"\\}");
        std::sregex_iterator iter(req.body.begin(), req.body.end(), e);
        std::sregex_iterator end;
        
        int count = 0;
        while (iter != end) {
            std::string extractedKey = (*iter)[1];
            std::string extractedVal = (*iter)[2];
            
            // Naya node seedha apni local RAM (Vault) mein data save kar lega
            this->storage->put(extractedKey, extractedVal);
            count++;
            ++iter;
        }

        std::cout << "[TRANSFER] Successfully absorbed " << count << " donated keys into local storage.\n";
        res.status = 200;
        res.set_content("Transfer Complete", "text/plain");
    });

    // 6. REPLICATION API
    server.Post("/internal/replicate", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        wal->appendLog("PUT", key, value);
        storage->put(key, value);
        res.status = 200;
    });

    // 7. PUBLIC PUT API (Dynamic Quorum Enforcer)
    server.Post("/put", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        if (!req.has_param("key") || !req.has_param("value")) {
            res.status = 400;
            res.set_content(R"({"error": "Missing key or value"})", "application/json");
            return;
        }

        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        int ttl = req.has_param("ttl") ? std::stoi(req.get_param_value("ttl")) : 0;
        
        int current_W;
        {
            std::shared_lock<std::shared_mutex> lock(global_config.config_lock);
            current_W = global_config.W;
        }

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

            if (acks >= current_W) {
                res.status = 200;
                res.set_content(R"({"status": "success"})", "application/json");
            } else {
                storage->remove(key); 
                wal->appendLog("DELETE", key, ""); 
                res.status = 503; 
                res.set_content(R"({"error": true, "message": "Write Quorum Failed: Needed )" + std::to_string(current_W) + R"(, Got )" + std::to_string(acks) + R"("})", "application/json");
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

    // 8. INTERNAL PUT API
    server.Post("/internal/put", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        int ttl = req.has_param("ttl") ? std::stoi(req.get_param_value("ttl")) : 0;
        wal->appendLog("PUT", key, value);
        storage->put(key, value, ttl);
        res.status = 200;
        res.set_content("Internal Backup Saved", "text/plain");
    });

    // 9. PUBLIC GET API
    server.Get("/get", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        if (!req.has_param("key")) {
            res.status = 400;
            res.set_content("Missing 'key'", "text/plain");
            return;
        }

        std::string key = req.get_param_value("key");
        
        int current_R;
        {
            std::shared_lock<std::shared_mutex> lock(global_config.config_lock);
            current_R = global_config.R;
        }

        if (current_R == 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2)); 
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(current_R * 12)); 
        }

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

    // 10. PUBLIC DELETE API
    server.Delete("/delete", [this](const httplib::Request& req, httplib::Response& res) {
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

    // 11. INTERNAL DELETE API
    server.Delete("/internal/delete", [this](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        wal->appendLog("DELETE", key, "");
        storage->remove(key);
        res.status = 200;
        res.set_content("Backup Deleted", "text/plain");
    });

    // 12. ADMIN API: Cluster Status (Frontend Heartbeat)
    server.Get("/admin/status", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        std::string jsonResponse = R"({
            "nodes": [
                {"id": 8080, "angle": 0, "status": "alive", "color": "#06b6d4"},
                {"id": 8082, "angle": 120, "status": "alive", "color": "#10b981"},
                {"id": 8084, "angle": 240, "status": "alive", "color": "#8b5cf6"}
            ],
            "dataMap": []
        })";
        
        res.set_content(jsonResponse, "application/json");
    });
}

// ==========================================
// 🚀 THE HTTP SENDER LOGIC: Purana Node Data Pack Karke Bhejega
// ==========================================
void Router::initiateRebalance(const std::string& newNodeAddress) {
    std::cout << "\n[REBALANCE] 🔍 Scanning local storage for keys belonging to " << newNodeAddress << "...\n";
    
    // 1. Vault se saara data nikalna (Shared Lock ke sath safely)
    auto allData = storage->getAllData();
    
    std::string jsonBody = "[";
    bool isFirst = true;
    int transferCount = 0;

    // 2. Loop chalakar check karna kaunsi key donate karni hai
    for (const auto& pair : allData) {
        std::string key = pair.first;
        std::string val = pair.second;

        // Hash Ring se pucho: "Kya is key ka naya malik 'newNodeAddress' hai?"
        std::string currentOwner = hashRing->getOwnerNode(key);
        
        if (currentOwner == newNodeAddress) {
            if (!isFirst) jsonBody += ",";
            
            // JSON String build karna: {"k":"user_1", "v":"delhi"}
            jsonBody += "{\"k\":\"" + key + "\",\"v\":\"" + val + "\"}";
            isFirst = false;
            transferCount++;

            // 3. Apni memory se delete karna (Kyunki ab yeh data donate ho raha hai)
            storage->remove(key);
        }
    }
    jsonBody += "]";

    // Agar koi data transfer ke liye nahi mila toh yahi ruk jao
    if (transferCount == 0) {
        std::cout << "[REBALANCE] No keys needed to be transferred. Node is balanced.\n";
        return;
    }

    std::cout << "[REBALANCE] 📦 Packing " << transferCount << " keys. Dispatching to " << newNodeAddress << "...\n";

    // 4. Target node ka host aur port nikalna
    size_t colonPos = newNodeAddress.find(':');
    if (colonPos == std::string::npos) return;
    
    std::string host = newNodeAddress.substr(0, colonPos);
    int port = std::stoi(newNodeAddress.substr(colonPos + 1));

    // 5. cpp-httplib ka client use karke naye node par POST marna
    httplib::Client cli(host, port);
    cli.set_connection_timeout(5, 0); // Bulk transfer mein thoda time lag sakta hai
    
    auto res = cli.Post("/internal/transfer", jsonBody, "application/json");

    if (res && res->status == 200) {
        std::cout << "[REBALANCE] ✅ Data donation successful!\n";
    } else {
        std::cerr << "[REBALANCE] ❌ Failed to transfer data. Target might be down.\n";
    }
}

void Router::start(int port) {
    std::cout << "[Router] API Gateway starting on port " << port << "..." << std::endl;
    server.listen("0.0.0.0", port);
}