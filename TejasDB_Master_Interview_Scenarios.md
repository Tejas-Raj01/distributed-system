# 🚀 Tejas-DB: Master Interview Scenarios & Input/Output Guide

> **Obsidian Note**  
> **Tags:** `#distributed-systems` `#interview-prep` `#cpp` `#system-design` `#scenarios` `#architecture`  
> **Project:** Tejas-DB  
> **Author:** Tejas Raj  

---

## 📌 Introduction & Overview

This document provides an **exhaustive, 360-degree walkthrough of every execution scenario, input/output contract, failure mode, and edge case** in **Tejas-DB**. 

Whether an interviewer asks about basic CRUD workflows, thread synchronization, disk persistence, network partitions, dynamic rebalancing, or chaos engineering, this guide gives you the exact technical explanation, data flow, code reference, and expected HTTP/IPC outputs.

---

## 🗂️ Table of Contents

1. [Core Data Operations (CRUD Scenarios)](#1-core-data-operations-crud-scenarios)
   * 1.1 Standard Write (`POST /put`) on Primary Owner Node
   * 1.2 Proxied Write (`POST /put`) via Non-Owner Node
   * 1.3 Write Quorum Failure & Rollback Scenario
   * 1.4 Standard Read (`GET /get`) on Primary Owner Node
   * 1.5 Read Non-Existent Key (`404 Not Found`)
   * 1.6 Lazy Eviction on Expired Read (`TTL Expiration`)
   * 1.7 Public & Replica Key Deletion (`DELETE /delete`)
2. [Storage Engine, LRU & Concurrency Scenarios](#2-storage-engine-lru--concurrency-scenarios)
   * 2.1 Storage Capacity Exhaustion & LRU Eviction (10,000 Keys Limit)
   * 2.2 Active Background TTL Garbage Collector
   * 2.3 Reader-Writer Lock Concurrency (`std::shared_mutex`)
3. [Crash Recovery & Persistence Scenarios (WAL)](#3-crash-recovery--persistence-scenarios-wal)
   * 3.1 Cold Boot Recovery from Disk Log File
   * 3.2 Sudden Node Crash & Unflushed Write Safety
4. [Cluster Discovery, Gossip & Rebalancing Scenarios](#4-cluster-discovery-gossip--rebalancing-scenarios)
   * 4.1 Seed Node Bootstrap & Peer Discovery (`/internal/gossip`)
   * 4.2 Dynamic Node Joining & Data Donation (`/internal/transfer`)
   * 4.3 Heartbeat Failure Detection & Node Eviction (3 Failures = Dead)
5. [Admin, Orchestration & Chaos Engineering Scenarios](#5-admin-orchestration--chaos-engineering-scenarios)
   * 5.1 Dynamic Consensus Configuration Update (`POST /admin/config`)
   * 5.2 Automated Background Process Spawning (`POST /admin/spawn`)
   * 5.3 Chaos Engineering Node Termination (`POST /admin/kill`)
   * 5.4 Complete Cluster Memory & Disk Wipe (`POST /admin/clear`)
   * 5.5 CORS Preflight & Browser Security Handling (`OPTIONS`)
6. [Master Interview Q&A Cheatsheet](#6-master-interview-qa-cheatsheet)

---

## 1. Core Data Operations (CRUD Scenarios)

### Scenario 1.1: Standard Write (`POST /put`) on Primary Owner Node

* **Context:** Client sends a write request directly to the node that owns the key's hash token.
* **Input Request:**
  * **HTTP Method:** `POST`
  * **Endpoint:** `/put`
  * **Headers:** `Content-Type: application/x-www-form-urlencoded`
  * **Body Params:** `key=user:101&value=Tejas&ttl=60`
* **Internal Execution Step-by-Step:**
  1. **Gateway Ingress ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L130-L180)):** Reads active consensus configuration under `global_config.config_lock` (`W=2`, `N=3`).
  2. **Hash Ring Evaluation ([ConsistentHash.cpp](file:///home/devian/Projects/distributed-system/src/core/ConsistentHash.cpp)):** Calculates `FNV-1a("user:101")` and looks up the ring. Ring determines `ownerNode == myAddress` (`127.0.0.1:8080`).
  3. **Disk Durability ([WAL.cpp](file:///home/devian/Projects/distributed-system/src/core/WAL.cpp)):** Appends `"PUT,user:101,Tejas\n"` to `data/wal_8080.log` under `file_lock` and executes `logFile.flush()`.
  4. **In-Memory Write ([StorageEngine.cpp](file:///home/devian/Projects/distributed-system/src/core/StorageEngine.cpp#L60-L84)):** Acquires exclusive write lock (`std::unique_lock<std::shared_mutex>`), calculates `expiry = currentTime + 60s`, inserts key into `dataStore`, updates `lruQueue` and `lruMap`. Sets `acks = 1`.
  5. **Synchronous Peer Replication ([Replicator.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Replicator.cpp#L7-L37)):**
     * Finds active physical nodes (excluding self).
     * Sends `POST /internal/replicate` with `key=user:101&value=Tejas&ttl=60` to replica nodes (`127.0.0.1:8081` and `127.0.0.1:8082`) with a 2-second timeout.
     * Replica nodes append to their local WAL, update their local memory, and return `200 OK`.
     * Owner increments `acks++` (reaches `acks = 2`).
  6. **Quorum Verification:** Checks `acks (2) >= effective_W (min(2, 3))`. Quorum satisfied!
* **Output Response:**
  * **HTTP Status:** `200 OK`
  * **Content-Type:** `application/json`
  * **Response Body:** `{"status": "success"}`

---

### Scenario 1.2: Proxied Write (`POST /put`) via Non-Owner Node

* **Context:** Client connects to Node 8081, but Consistent Hashing assigns ownership of `"user:101"` to Node 8080.
* **Input Request:**
  * **Target Node:** `http://127.0.0.1:8081/put`
  * **Body Params:** `key=user:101&value=Tejas`
* **Internal Execution Step-by-Step:**
  1. **Hash Ring Evaluation:** Node 8081 computes `hashRing->getOwnerNode("user:101")` => returns `"127.0.0.1:8080"`.
  2. **Proxy Decision:** Node 8081 detects `ownerNode ("127.0.0.1:8080") != myAddress ("127.0.0.1:8081")`.
  3. **HTTP Client Proxying ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L181-L196)):**
     * Node 8081 opens HTTP client `httplib::Client("http://127.0.0.1:8080")`.
     * Forwards `POST /put` with original parameters.
  4. **Owner Node Execution:** Node 8080 executes WAL write, memory mutation, synchronous replication (`W=2`), and returns `200 OK` to Node 8081.
  5. **Response Relay:** Node 8081 relays Node 8080's HTTP status and JSON payload back to the client.
* **Output Response:**
  * **HTTP Status:** `200 OK`
  * **Response Body:** `{"status": "success"}`

---

### Scenario 1.3: Write Quorum Failure & Memory Rollback

* **Context:** Network partition or double replica crash prevents owner node from receiving enough replication ACKs.
* **Setup:** Cluster has `N=3, W=2`. Peer nodes `8081` and `8082` are offline or timing out.
* **Input Request:** `POST /put` with `key=user:101&value=Tejas` sent to Owner Node `8080`.
* **Internal Execution Step-by-Step:**
  1. Owner node writes locally to WAL and memory (`acks = 1`).
  2. Replicator attempts synchronous HTTP calls to `8081` and `8082`.
  3. Both network calls hit the 2-second timeout (`cli.set_read_timeout(2, 0)`). Returns `false`.
  4. Total ACKs remain `acks = 1`.
  5. Quorum Check: `acks (1) < effective_W (2)`. Quorum failed!
  6. **Compensating Rollback ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L176-L179)):**
     * Calls `storage->remove("user:101")` to clear uncommitted in-memory state.
     * Appends `"DELETE,user:101,"` tombstone to WAL to maintain disk-memory parity.
* **Output Response:**
  * **HTTP Status:** `503 Service Unavailable`
  * **Content-Type:** `application/json`
  * **Response Body:** `{"error": true, "message": "Write Quorum Failed"}`

---

### Scenario 1.4: Standard Read (`GET /get`) on Primary Owner Node

* **Context:** Client reads key `"user:101"`.
* **Input Request:**
  * **HTTP Method:** `GET`
  * **Endpoint:** `/get?key=user:101`
* **Internal Execution Step-by-Step:**
  1. **Read Quorum Latency Simulation ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L223-L233)):** Reads `current_R` from `global_config`. Since `current_R = 2`, executes `std::this_thread::sleep_for(24ms)` to simulate read path quorum evaluation.
  2. **Owner Verification:** Computes `getOwnerNode("user:101")` => matches `myAddress`.
  3. **Storage Engine Lookup ([StorageEngine.cpp](file:///home/devian/Projects/distributed-system/src/core/StorageEngine.cpp#L87-L108)):**
     * Acquires exclusive lock `std::unique_lock<std::shared_mutex>` (needed to update LRU list ordering).
     * Verifies key exists and `expiry_time` is valid (or 0).
     * Promotes `"user:101"` to the front of `lruQueue` and updates iterator in `lruMap`.
     * Returns `std::optional<std::string>("Tejas")`.
* **Output Response:**
  * **HTTP Status:** `200 OK`
  * **Content-Type:** `text/plain`
  * **Response Body:** `Tejas`

---

### Scenario 1.5: Read Non-Existent Key (`404 Not Found`)

* **Input Request:** `GET /get?key=unknown_key`
* **Internal Execution Step-by-Step:**
  1. Evaluates owner node and routes request to owner.
  2. `StorageEngine::get("unknown_key")` searches `dataStore`. Key not found in `std::unordered_map`.
  3. Returns `std::nullopt`.
* **Output Response:**
  * **HTTP Status:** `404 Not Found`
  * **Content-Type:** `text/plain`
  * **Response Body:** `Key Not Found`

---

### Scenario 1.6: Lazy Eviction on Expired Read (TTL Expiration)

* **Context:** Key `"user:101"` was stored with `ttl=5`. 10 seconds have passed, but background garbage collector hasn't cleaned it yet.
* **Input Request:** `GET /get?key=user:101`
* **Internal Execution Step-by-Step:**
  1. `StorageEngine::get("user:101")` finds key in `dataStore`.
  2. Checks condition: `expiry_time > 0 && expiry_time < getCurrentTime()`. Condition TRUE!
  3. **Lazy Eviction:** Instantly removes key from `lruQueue`, `lruMap`, and `dataStore`.
  4. Returns `std::nullopt`.
* **Output Response:**
  * **HTTP Status:** `404 Not Found`
  * **Content-Type:** `text/plain`
  * **Response Body:** `Key Not Found`

---

### Scenario 1.7: Public & Replica Key Deletion (`DELETE /delete`)

* **Input Request:**
  * **HTTP Method:** `DELETE`
  * **Endpoint:** `/delete?key=user:101`
* **Internal Execution Step-by-Step:**
  1. **Local Deletion ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L260-L288)):**
     * Calls `storage->remove("user:101")`. Erases key from memory and LRU structures.
     * Appends `"DELETE,user:101,"` tombstone to WAL.
  2. **Asynchronous Cluster Delete Forwarding ([Replicator.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Replicator.cpp#L40-L59)):**
     * Iterates over all active nodes in ring.
     * For each peer, sends `DELETE /internal/delete?key=user:101`.
     * Peer nodes receive request, write tombstone to local WAL, and erase from local memory.
* **Output Response:**
  * **HTTP Status:** `200 OK`
  * **Response Body:** `Key Deleted Successfully from cluster`

---

## 2. Storage Engine, LRU & Concurrency Scenarios

### Scenario 2.1: Cache Capacity Exhaustion & LRU Eviction

* **Context:** Engine capacity is initialized to `capacity = 10,000` keys. `dataStore` currently holds 10,000 active keys.
* **Action:** Client writes 10,001st key `PUT("user:10001", "Data")`.
* **Internal Execution Step-by-Step ([StorageEngine.cpp](file:///home/devian/Projects/distributed-system/src/core/StorageEngine.cpp#L77-L84)):**
  1. Writer acquires lock. Evaluates `dataStore.size() >= capacity` (10,000 >= 10,000).
  2. Invokes `evictOldest()`:
     * Identifies tail key of `std::list<std::string> lruQueue` (least recently accessed key, e.g., `"user:1"`).
     * Removes `"user:1"` from `lruQueue`.
     * Erases `"user:1"` from `lruMap` iterator lookup.
     * Erases `"user:1"` from main `dataStore` map.
  3. Inserts `"user:10001"` at head of `lruQueue` and updates `dataStore`.

---

### Scenario 2.2: Active Background TTL Garbage Collector

* **Context:** Background thread `cleanupTask` runs continuously in `StorageEngine`.
* **Execution Loop ([StorageEngine.cpp](file:///home/devian/Projects/distributed-system/src/core/StorageEngine.cpp#L26-L48)):**
  1. Thread sleeps for `std::chrono::seconds(1)`.
  2. Wakes up and acquires exclusive Write Lock (`std::unique_lock<std::shared_mutex>`).
  3. Fetches current UNIX epoch timestamp in seconds.
  4. Scans `dataStore` map. For any entry matching `expiry_time > 0 && expiry_time < now`:
     * Prints log: `[TTL] Key expired and evicted: key_name`.
     * Erases from `lruQueue` via `lruMap` iterator pointer in O(1) time.
     * Erases from `lruMap` and `dataStore`.
  5. Releases lock and sleeps for another 1 second.

---

### Scenario 2.3: Reader-Writer Lock Concurrency (`std::shared_mutex`)

* **Context:** Multiple HTTP threads in `cpp-httplib` handle incoming requests concurrently.
* **Concurrency Rules:**
  * **Multiple Concurrent `getAllData()` Calls:** Acquire `std::shared_lock<std::shared_mutex>`. Read operations proceed in parallel without blocking each other.
  * **`PUT`, `DELETE`, or Eviction Operations:** Acquire `std::unique_lock<std::shared_mutex>`. Blocks all concurrent readers and writers, preventing data races and iterator invalidation.

---

## 3. Crash Recovery & Persistence Scenarios (WAL)

### Scenario 3.1: Cold Boot Recovery from Disk Log File

* **Context:** Node 8080 was restarted after maintenance or system shutdown. Disk file `data/wal_8080.log` exists on disk.
* **Log File Content Sample:**
  ```text
  PUT,user:101,Tejas
  PUT,user:102,Raj
  DELETE,user:101,
  PUT,user:103,System
  ```
* **Boot Execution Step-by-Step ([main.cpp](file:///home/devian/Projects/distributed-system/src/main.cpp#L35-L41) & [WAL.cpp](file:///home/devian/Projects/distributed-system/src/core/WAL.cpp)):**
  1. `main()` initializes `StorageEngine storage(10000)` and `WAL wal("data/wal_8080.log")`.
  2. Executes `wal.recover(storage)`.
  3. `WAL::recover` opens `data/wal_8080.log` in read mode and parses line-by-line:
     * Line 1: `PUT,user:101,Tejas` => calls `storage.put("user:101", "Tejas")`.
     * Line 2: `PUT,user:102,Raj` => calls `storage.put("user:102", "Raj")`.
     * Line 3: `DELETE,user:101,` => calls `storage.remove("user:101")`.
     * Line 4: `PUT,user:103,System` => calls `storage.put("user:103", "System")`.
  4. Memory state is fully restored to exact crash point before server starts accepting HTTP connections!

---

### Scenario 3.2: Sudden Node Crash & Flush Durability

* **Question:** What happens if OS power fails 1 millisecond after a `PUT` request completes?
* **Safety Mechanism ([WAL.cpp](file:///home/devian/Projects/distributed-system/src/core/WAL.cpp)):**
  * In `WAL::appendLog()`, every write executes `logFile << operation << "," << key << "," << value << "\n";` followed immediately by `logFile.flush();`.
  * `flush()` forces OS buffer page caches to sync to hardware persistent disk.
  * Therefore, zero uncommitted writes are lost upon process crash.

---

## 4. Cluster Discovery, Gossip & Rebalancing Scenarios

### Scenario 4.1: Seed Node Bootstrap & Peer Discovery

* **Context:** Server Node 8082 starts up and joins an existing cluster with seed node `127.0.0.1:8080`.
* **Command:** `./build/kv_server 8082 127.0.0.1:8080`
* **Internal Execution Step-by-Step ([Gossip.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Gossip.cpp#L16-L32)):**
  1. Node 8082 adds itself to its local Consistent Hash ring (100 virtual nodes).
  2. Node 8082 loops over seed nodes and invokes `sendDiscoveryPing("127.0.0.1:8080")`.
  3. Sends `POST http://127.0.0.1:8080/internal/gossip` with body param `newNode=127.0.0.1:8082`.
  4. Node 8080 receives message in `receiveGossip()`:
     * Adds `127.0.0.1:8082` to its peer list and Consistent Hash ring.
     * Logs: `🚨 ALERT: New Node Joined the Cluster -> 127.0.0.1:8082!`.

---

### Scenario 4.2: Dynamic Node Joining & Data Donation (Rebalance)

* **Context:** When Node 8082 joins, the Consistent Hash ring tokens shift. Some keys currently stored on Node 8080 now belong to Node 8082.
* **Internal Rebalance Execution ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L442-L488)):**
  1. Node 8080 detaches a background thread to run `initiateRebalance("127.0.0.1:8082")`.
  2. Node 8080 iterates through its local `storage->getAllData()`.
  3. For each key, evaluates `currentOwner = hashRing->getOwnerNode(key)`.
  4. If `currentOwner == "127.0.0.1:8082"`:
     * Packs key-value pair into JSON array: `[{"k":"user:101","v":"Tejas"}]`.
     * Removes key from local storage (`storage->remove(key)`).
  5. Sends JSON payload via HTTP POST to `http://127.0.0.1:8082/internal/transfer`.
  6. Node 8082 absorbs donated keys into its local storage via regex parsing. Data rebalanced flawlessly without client downtime!

---

### Scenario 4.3: Heartbeat Failure Detection & Node Eviction

* **Context:** Node 8081 crashes or experiences network isolation.
* **Heartbeat Execution ([Gossip.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Gossip.cpp#L101-L149)):**
  1. Node 8080's background thread `runHeartbeat()` wakes every 2 seconds.
  2. Sends `GET http://127.0.0.1:8081/ping` with a 1-second connection timeout.
  3. Connection fails. Node 8080 increments `failureCount["127.0.0.1:8081"]++`.
  4. Logs: `[Gossip] Missed heartbeat for 127.0.0.1:8081 (Failures: 1/3)`.
  5. On 3rd consecutive failed ping (6 seconds elapsed):
     * Logs: `🚨 Node 127.0.0.1:8081 is DEAD! Evicting from cluster.`.
     * Calls `hashRing->removeNode("127.0.0.1:8081")` (removes all 100 vNodes).
     * Erases Node 8081 from `peers` vector.
     * Ring updates automatically. Subsequent requests route to surviving replicas!

---

## 5. Admin, Orchestration & Chaos Engineering Scenarios

### Scenario 5.1: Dynamic Consensus Configuration Update

* **Input Request:**
  * **HTTP Method:** `POST`
  * **Endpoint:** `/admin/config`
  * **JSON Body:** `{"N": 5, "W": 3, "R": 3}`
* **Internal Execution ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L52-L74)):**
  1. Parses JSON parameters `N`, `W`, `R`.
  2. Acquires write lock on `global_config.config_lock`.
  3. Updates `global_config.N = 5`, `global_config.W = 3`, `global_config.R = 3`.
* **Output Response:**
  * **HTTP Status:** `200 OK`
  * **Response Body:** `{"status":"success"}`

---

### Scenario 5.2: Automated Background Process Spawning

* **Input Request:**
  * **HTTP Method:** `POST`
  * **Endpoint:** `/admin/spawn?port=8086`
* **Internal Execution ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L352-L376)):**
  1. Validates `port` parameter is numeric.
  2. Formulates shell command: `./build/kv_server 8086 127.0.0.1:8080 > data/node_8086.log 2>&1 &`.
  3. Invokes `std::system(cmd.c_str())` to spawn background OS process.
* **Output Response:**
  * **HTTP Status:** `200 OK`
  * **Response Body:** `{"status": "success", "message": "Node spawned successfully"}`

---

### Scenario 5.3: Chaos Engineering Node Termination

* **Input Request:**
  * **HTTP Method:** `POST`
  * **Endpoint:** `/admin/kill?port=8081`
* **Internal Execution ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L379-L402)):**
  1. Checks target port. If `targetPort == "8080"`, rejects call with HTTP `400` (`Cannot kill master node`).
  2. Formulates shell command: `pkill -f 'kv_server 8081'`.
  3. Invokes `std::system(cmd.c_str())` to terminate node process.
* **Output Response:**
  * **HTTP Status:** `200 OK`
  * **Response Body:** `{"status": "success", "message": "Node terminated"}`

---

### Scenario 5.4: Complete Cluster Memory & Disk Wipe

* **Input Request:**
  * **HTTP Method:** `POST`
  * **Endpoint:** `/admin/clear`
* **Internal Execution ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L424-L439)):**
  1. Calls `storage->clearAll()`, acquiring write lock and clearing `dataStore`, `lruQueue`, and `lruMap`.
  2. Executes `std::system("rm -f data/*")` to wipe disk WAL files.
* **Output Response:**
  * **HTTP Status:** `200 OK`
  * **Response Body:** `{"status": "success", "message": "Cluster memory wiped clean!"}`

---

### Scenario 5.5: CORS Preflight & Browser Security Handling

* **Input Request:**
  * **HTTP Method:** `OPTIONS`
  * **Endpoint:** Any endpoint (e.g., `/put`, `/get`, `/admin/status`)
* **Internal Execution ([Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L32-L42)):**
  1. Intercepted by `server.set_pre_routing_handler`.
  2. Attaches headers:
     * `Access-Control-Allow-Origin: *`
     * `Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS`
     * `Access-Control-Allow-Headers: Content-Type, Authorization, ngrok-skip-browser-warning`
  3. Short-circuits routing and returns immediately.
* **Output Response:**
  * **HTTP Status:** `204 No Content`

---

## 6. Master Interview Q&A Cheatsheet

| Scenario / Question | Technical Answer & Code Ref |
| :--- | :--- |
| **How do you handle hash hotspot uneven distribution?** | Virtual Nodes (100 vNodes per physical node in [ConsistentHash.cpp](file:///home/devian/Projects/distributed-system/src/core/ConsistentHash.cpp)). Interleaves vNode tokens across 64-bit FNV-1a ring. |
| **Is replication synchronous or asynchronous?** | Synchronous for Write Quorum (`W=2`). [Replicator.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Replicator.cpp) blocks with 2-second timeout until `W` ACKs are received. |
| **What happens if WAL write fails or power fails?** | WAL uses synchronous `logFile.flush()` on every operation. On boot, `WAL::recover` replays the log file into memory. |
| **How is LRU eviction thread-safe?** | `StorageEngine` uses `std::shared_mutex`. `get()` and `put()` acquire exclusive `std::unique_lock` to reorder `std::list lruQueue` and update `lruMap` pointers safely. |
| **What failure detector algorithm is used?** | Heartbeat Ping Gossip protocol in [Gossip.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Gossip.cpp). Runs every 2 seconds; 3 consecutive missed pings (6s) triggers node eviction and dynamic rebalance. |
