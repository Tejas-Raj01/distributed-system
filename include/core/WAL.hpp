#pragma once
#include <string>
#include <fstream>
#include <mutex>

// Forward declaration taaki compiler ko pata ho ki yeh class exist karti hai
class StorageEngine; 

class WAL {
private:
    std::string filepath;
    std::ofstream logFile;
    std::mutex file_lock; // Disk writes ko thread-safe banane ke liye

public:
    // Constructor jahan file ka naam set hoga aur file open hogi
    WAL(const std::string& path);
    
    // Destructor mein file close karna zaruri hai
    ~WAL();

    // Log likhne ka function
    void appendLog(const std::string& action, const std::string& key, const std::string& value = "");

    // Server start hone par purana data RAM mein wapas laane ka function
    void recover(StorageEngine& engine);
};