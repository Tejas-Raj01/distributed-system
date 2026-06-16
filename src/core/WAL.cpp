#include "../../include/core/WAL.hpp"
#include "../../include/core/StorageEngine.hpp" // Yahan poori definition chahiye kyunki recover() mein functions call honge
#include <iostream>
#include <sstream>

// Constructor: File ko 'Append' mode mein open karega
WAL::WAL(const std::string& path) : filepath(path) {
    // std::ios::app ensures ki naya data hamesha file ke end mein add ho, purana delete na ho
    logFile.open(filepath, std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Error: WAL file open nahi ho saki!" << std::endl;
    }
}

// Destructor
WAL::~WAL() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

// Action ko file mein likhna
void WAL::appendLog(const std::string& action, const std::string& key, const std::string& value) {
    // Exclusive lock taaki ek waqt par ek hi thread file mein likh sake
    std::lock_guard<std::mutex> lock(file_lock);

    // Format: ACTION,KEY,VALUE
    // Example: PUT,user_1,Ramesh
    logFile << action << "," << key << "," << value << "\n";
    
    // CRITICAL: flush() ensures ki OS data ko RAM mein buffer na kare, seedha Hard Disk par utaar de.
    logFile.flush(); 
}

// Crash Recovery: Server chalu hote hi RAM ko wapas populate karna
void WAL::recover(StorageEngine& engine) {
    std::ifstream inFile(filepath);
    if (!inFile.is_open()) {
        return; // Agar file nahi hai, toh matlab server pehli baar start ho raha hai
    }

    std::string line;
    // File ko line-by-line padhna
    while (std::getline(inFile, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string action, key, value;

        // Comma (,) ko delimiter banakar string ko todna
        std::getline(ss, action, ',');
        std::getline(ss, key, ',');
        std::getline(ss, value); // Bacha hua hissa value mein jayega

        // WAL se padh kar wapas StorageEngine (RAM) mein daalna
        if (action == "PUT") {
            engine.put(key, value);
        } 
        else if (action == "REMOVE") {
            engine.remove(key);
        }
    }
    
    inFile.close();
    std::cout << "Recovery successful! Data restored from " << filepath << std::endl;
}