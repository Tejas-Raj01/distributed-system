#include "../../include/core/WAL.hpp"
#include "../../include/core/StorageEngine.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>

// Opens WAL file in append mode
WAL::WAL(const std::string& path) : filepath(path) {
    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
    // std::ios::app ensures new entries are appended without overwriting existing data
    logFile.open(filepath, std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Error: Could not open WAL file!" << std::endl;
    }
}

WAL::~WAL() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

// Appends an entry to the log file (Format: ACTION,KEY,VALUE)
void WAL::appendLog(const std::string& action, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(file_lock);

    logFile << action << "," << key << "," << value << "\n";
    
    // CRITICAL: flush() ensures data is immediately flushed to disk
    logFile.flush(); 
}

// Crash Recovery: replays WAL entries into memory on startup
void WAL::recover(StorageEngine& engine) {
    std::ifstream inFile(filepath);
    if (!inFile.is_open()) {
        return; // File does not exist yet (first startup)
    }

    std::string line;
    while (std::getline(inFile, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string action, key, value;

        // Parse CSV format: ACTION,KEY,VALUE
        std::getline(ss, action, ',');
        std::getline(ss, key, ',');
        std::getline(ss, value);

        if (action == "PUT") {
            engine.put(key, value);
        } 
        else if (action == "DELETE") {
            engine.remove(key);
        }
    }
    
    inFile.close();
    std::cout << "Recovery successful! Data restored from " << filepath << std::endl;
}