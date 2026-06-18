#pragma once
#include <shared_mutex>
#include <mutex>

struct ClusterConfig {
    // Default Quorum Rules
    int N = 3; // Replication Factor (Total copies)
    int W = 2; // Write Quorum (Min nodes to ack write)
    int R = 2; // Read Quorum (Min nodes to ack read)
    
    // Read-Write Lock for thread-safe config updates during high traffic
    mutable std::shared_mutex config_lock; 

    // Helper functions thread-safe updates ke liye
    void update(int n, int w, int r) {
        std::unique_lock<std::shared_mutex> lock(config_lock);
        if (n > 0) N = n;
        if (w > 0) W = w;
        if (r > 0) R = r;
    }
};

// Global instance declaration (Isko main.cpp mein define karenge)
extern ClusterConfig global_config;