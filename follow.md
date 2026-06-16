# 🏗️ Architecture Blueprint: C++ Distributed Key-Value Store

## 🎯 Project Overview
Yeh project ek highly scalable, distributed, in-memory key-value store hai jise 100% modern C++ (C++17/20) mein scratch se banaya ja raha hai. Iska main objective Google ke JD ki core requirements ko hit karna hai: **Concurrency, Multi-threading, Synchronization, aur Distributed Systems**. 

Node.js ke single-threaded nature ko chhod kar, hum isme actual OS-level threads (`std::thread`) aur advanced locking mechanisms (`std::shared_mutex`) ka use karenge.

---

## 📂 Directory Structure (Pitchfork Layout)

Yeh C++ ka industry-standard structure hai:

\`\`\`text
distributed-kv-cpp/
├── CMakeLists.txt          # C++ code ko compile karne ka main build engine
├── external/               # Third-party "Header-only" files yahan rahengi
│   ├── httplib.h           # (HTTP Server/Client banane ke liye)
│   └── json.hpp            # (JSON ko parse aur stringify karne ke liye)
├── include/                # 🧠 HEADERS (.hpp) - Yahan sirf Blueprint/Declarations hongi
│   ├── core/
│   │   ├── StorageEngine.hpp
│   │   ├── ConsistentHash.hpp
│   │   └── WAL.hpp
│   ├── cluster/
│   │   ├── Gossip.hpp
│   │   └── Replicator.hpp
│   └── network/
│       └── Router.hpp
├── src/                    # ⚙️ IMPLEMENTATION (.cpp) - Asli logic aur algorithms yahan honge
│   ├── core/
│   │   ├── StorageEngine.cpp
│   │   ├── ConsistentHash.cpp
│   │   └── WAL.cpp
│   ├── cluster/
│   │   ├── Gossip.cpp
│   │   └── Replicator.cpp
│   ├── network/
│   │   └── Router.cpp
│   └── main.cpp            # Entry Point (int main) jahan se sab start hoga
└── data/                   # Crash recovery ke liye `.log` (WAL) files yahan save hongi
\`\`\`

---

## 🧠 Module 1: The Core Data Layer (`core/`)
Yeh layer database ka dimaag hai. Yahan actual data store hoga aur hash math calculate hoga.

### 1. `StorageEngine.hpp` & `.cpp` (The Thread-Safe Database)
*   **Kaam (Purpose):** Yeh In-Memory RAM database hai. Isme hum LRU (Least Recently Used) cache policy lagayenge taaki memory full hone par purana data apne aap delete ho jaye.
*   **Internal State (Variables):**
    *   `std::unordered_map<std::string, std::string> dataStore`: Yeh asli hash map hai jahan `key` aur `value` save honge.
    *   `std::list<std::string> lruQueue`: Yeh doubly-linked list track karegi ki kaunsi key sabse nayi (recently used) hai aur kaunsi sabse purani.
    *   `std::unordered_map<std::string, std::list<std::string>::iterator> lruMap`: LRU list mein O(1) time complexity ke sath data dhoondhne ke liye.
    *   `std::shared_mutex rw_lock`: **(JD: Synchronization)** Yeh sabse important hai. Yeh 'Reader-Writer' lock hai. Ek waqt par 100 log data Read kar sakte hain, par Write karne ke time par sabko rukna padega.
*   **Core Functions:**
    *   `put(key, value)`: Pehle exclusive write-lock lagayega. Phir check karega ki memory limit cross toh nahi hui. Agar hui hai, toh LRU list se sabse purani key delete karega, naya data insert karega, aur lock khol dega.
    *   `get(key)`: Shared read-lock lagayega. Data return karega aur us key ko LRU list mein sabse aage (top) par le aayega.
    *   `remove(key)`: Data aur uske LRU trackers ko safely delete karega.

### 2. `ConsistentHash.hpp` & `.cpp` (The Math & Router)
*   **Kaam (Purpose):** 360-degree virtual ring banakar yeh decide karna ki kaunsa data kis server node (Port) par save hoga.
*   **Internal State (Variables):**
    *   `std::map<size_t, std::string> hashRing`: C++ mein `std::map` internally ek Binary Search Tree (BST) hota hai. Isme hum sorted order mein nodes ki position (0-360 degrees) aur unka address save karenge.
    *   `std::shared_mutex ring_lock`: Ring mein naya node add/remove karte waqt safe locking ke liye.
*   **Core Functions:**
    *   `size_t computeHash(key)`: String ko SHA-256 (ya custom hash) use karke ek bohot bade integer number mein badlega.
    *   `addNode(nodeAddress)`: Naye server ko ring mein jodeyga. (Virtual nodes bhi add karenge taaki load evenly distribute ho).
    *   `getOwnerNode(key)`: Key ka hash calculate karega, aur `hashRing.upper_bound(hash)` ka use karke ring par clockwise ghum kar batayega ki iska maalik (owner) kaunsa node hai.

### 3. `WAL.hpp` & `.cpp` (Write-Ahead Logging)
*   **Kaam (Purpose):** JD ki "Reliability" condition. Server crash/power cut hone par data udne se bachana.
*   **Internal State (Variables):**
    *   `std::ofstream logFile`: Hard disk par text file ko open rakhne ka pointer.
    *   `std::mutex file_lock`: Taaki do alag threads ek hi nanosecond mein file mein kuch ulta-seedha na likh dein.
*   **Core Functions:**
    *   `appendLog(action, key, value)`: `StorageEngine` mein data save hone se *thik pehle*, hard disk par ek line likhega (e.g., `PUT,apple,red\n`).
    *   `recover(StorageEngine& engine)`: Yeh sirf server start hone par ek baar chalega. Yeh `.log` file ko line-by-line padhega aur wapas data RAM mein bhar dega.

---

## 🌐 Module 2: The Cluster Layer (`cluster/`)
Yeh layer servers ko aapas mein ek "Swarm" (jhund) ki tarah kaam karna sikhayegi.

### 4. `Gossip.hpp` & `.cpp` (Failure Detection)
*   **Kaam (Purpose):** Background mein doosre servers ki heartbeat check karna.
*   **Internal State (Variables):**
    *   `std::thread heartbeatThread`: Yeh ek OS-level background thread hoga.
    *   `std::atomic<bool> isRunning`: Ek thread-safe boolean variable jisse hum loop ko safely band kar sakein.
*   **Core Functions:**
    *   `start()`: Ek naya thread launch karega jisme ek `while(isRunning)` loop chalega. Har 2 second mein yeh `httplib::Client` ka use karke baaki servers ko `GET /ping` bhejega. Agar koi server 3 baar lagatar fail hua, toh usko "Dead" mark karke Hash Ring se nikal dega.

### 5. `Replicator.hpp` & `.cpp` (Fault Tolerance / Backup)
*   **Kaam (Purpose):** Agar Primary Node crash ho jaye, toh data Secondary Node par available rahe.
*   **Core Functions:**
    *   `forwardToReplica(key, value, nextNode)`: Jaise hi kisi data ki request aayegi, yeh ek async task spawn karega jo HTTP POST request ke zariye next server ko us data ki ek copy (replica) bhej dega.

---

## 🔌 Module 3: Network Layer (`network/`)
Yeh C++ engine aur Frontend (React) ke beech ka bridge hai.

### 6. `Router.hpp` & `.cpp` (The API Gateway)
*   **Kaam (Purpose):** HTTP server run karna, incoming JSON/Text ko C++ variables mein parse karna.
*   **Internal State (Variables):**
    *   `httplib::Server server`: `cpp-httplib` ka main web server object.
*   **Core Functions:**
    *   `setupRoutes()`: Yahan API URLs (Endpoints) map honge:
        *   **POST `/put`**: Client se data aayega. Router `ConsistentHash` se poochega ki "Iska maalik kaun hai?". 
            *   *Scenario A:* Agar current server maalik hai, toh data ko `WAL` aur `StorageEngine` mein bhej dega.
            *   *Scenario B:* Agar maalik koi doosra node hai, toh Request us node ko proxy (forward) kar dega.
        *   **GET `/get/:key`**: Data laane ke liye same routing process follow karega.
        *   **GET `/ping`**: Gossip heartbeat ka jawab dega `200 OK` return karke.
    *   `start(port)`: Pura setup hone ke baad server ko diye gaye port par ON karega.

---

## 🚀 Module 4: The Entry Point

### 7. `main.cpp` (The Director)
*   **Execution Flow (Is file mein kya hoga?):**
    1. Command line se port number aayega (e.g., `./kv_server 8001`).
    2. `WAL.recover()` chalega taaki purana bacha hua data memory mein aa jaye.
    3. `ConsistentHash` initialize hoga aur ring banegi.
    4. `Gossip.start()` se background thread chalu ho jayega.
    5. `Router.setupRoutes()` API map karega, aur `Router.start(port)` system ko live kar dega.

### 8. `CMakeLists.txt` (The Build Script)
*   **Kaam:** C++ code ko compile karke machine binary (Executable) banana.
*   **Directives:**
    *   `CMAKE_CXX_STANDARD 17` set karenge.
    *   `Threads::Threads` link karenge (kyunki macOS/Linux/Windows mein threads ke OS level system calls alag hote hain).
    *   `include/` aur `external/` folder ko path mein batayenge taaki compiler ko header files mil jayein.