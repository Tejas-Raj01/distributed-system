#pragma once
#include <string>
#include <fstream>
#include <mutex>

// Forward declaration for StorageEngine
class StorageEngine; 

class WAL {
private:
    std::string filepath;
    std::ofstream logFile;
    std::mutex file_lock; // Mutex protecting disk writes

public:
    WAL(const std::string& path);
    ~WAL();

    // Appends an entry to the write-ahead log
    void appendLog(const std::string& action, const std::string& key, const std::string& value = "");

    // Recovers storage engine state by replaying the log on startup
    void recover(StorageEngine& engine);
};