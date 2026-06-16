#include "../../include/cluster/Replicator.hpp"
#include "../../external/httplib.h" // HTTP requests ke liye
#include <thread>    // Async threading ke liye
#include <iostream>

void Replicator::forwardToReplica(const std::string& key, const std::string& value, const std::string& nextNode) {
    if (nextNode.empty()) {
        return; // Agar koi replica node hi nahi hai, toh kuch mat karo
    }

    // std::thread + detach() ka use karke ek "Fire-and-Forget" background task banaya
    // Lambda function `[key, value, nextNode]()` in variables ki copy capture karta hai taaki thread safe rahe.
    std::thread([key, value, nextNode]() {
        
        // Next node (Replica) ke liye HTTP Client setup karo
        httplib::Client cli("http://" + nextNode);
        
        // Timeout chhota rakha hai (2 seconds) taaki resources block na hon
        cli.set_connection_timeout(2);
        cli.set_read_timeout(2);

        // Data ko POST request ke format mein set karo (Key-Value form data)
        httplib::Params params;
        params.emplace("key", key);
        params.emplace("value", value);

        // Replica server ke ek special internal API ("/internal/replicate") par POST request bhejo
        auto res = cli.Post("/internal/replicate", params);

        // Response check karo
        if (res && res->status == 200) {
            std::cout << "[Replicator] SUCCESS: Copied key '" << key 
                      << "' to replica -> " << nextNode << std::endl;
        } else {
            std::cerr << "[Replicator] FAILED: Could not copy key '" << key 
                      << "' to replica -> " << nextNode << std::endl;
        }

    }).detach(); // detach() main thread ko block nahi hone deta. Yeh background mein akele poora hoga.
}