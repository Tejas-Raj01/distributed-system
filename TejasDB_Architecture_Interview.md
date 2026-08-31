# 🎤 Distributed Key-Value Store: Interview Response Guide

> **Obsidian Note**  
> **Tags:** `#distributed-systems` `#interview-prep` `#cpp` `#system-design` `#architecture`  
> **Project:** Tejas-DB  

---

## ❓ Question 1: Architecture & End-to-End Request Lifecycle
**"Suppose I know nothing about your project. Explain the complete architecture of your distributed key-value database. Start from the moment a client sends `PUT("user:123", "Tejas")`. Walk me through exactly what happens—from receiving the request until the write is considered successful."**

---

## 🎙️ Executive Summary Answer (30-Second Elevator Pitch)

> "Tejas-DB is a **masterless, peer-to-peer distributed Key-Value store** built in C++17 inspired by Amazon Dynamo. It eliminates single points of failure using **Consistent Hashing** (64-bit FNV-1a algorithm with 100 virtual nodes per server), guarantees durability via **Write-Ahead Logging (WAL)**, delivers high-performance concurrency with a **thread-safe LRU Storage Engine** using reader-writer locks (`std::shared_mutex`), and enforces consistency using **Strict Quorum Consensus** ($W=2, R=2, N=3$).
>
> When a write request `PUT("user:123", "Tejas")` arrives, any node can act as the **Coordinator**. It computes the hash, routes the key to the responsible owner node, writes to disk-backed WAL first, mutates memory, synchronizes replicas over HTTP, evaluates write quorum ($W=2$), and returns a response to the client—rolling back if quorum fails."

---

## 🧱 Architectural Components Overview

Before diving into the execution flow, here is the architecture of each node in the cluster:

```
                  +-----------------------------------+
                  |        Client Application         |
                  +-----------------------------------+
                                    |
                                HTTP POST /put
                                    v
+-------------------------------------------------------------------+
|                        Tejas-DB Server Node                       |
|                                                                   |
|   +-----------------------------------------------------------+   |
|   | 1. Router Layer (Router.cpp & cpp-httplib)                |   |
|   |    - Handles HTTP REST API & CORS                         |   |
|   |    - Proxying & Request Routing                           |   |
|   +-----------------------------------------------------------+   |
|                                   |                               |
|   +-------------------------------+---------------------------+   |
|   |                               |                           |   |
|   v                               v                           v   |
| +--------------------+  +-------------------+  +--------------+ | |
| | 2. Consistent Hash |  | 3. Storage Engine |  | 4. WAL       | | |
| |    Ring            |  |    (In-Memory)    |  |    Engine    | | |
| |    - FNV-1a Hash   |  |    - std::map     |  |    - Append  | | |
| |    - 100 vNodes    |  |    - LRU Queue    |  |      Only    | | |
| |    - std::map      |  |    - shared_mutex |  |    - flush() | | |
| +--------------------+  +-------------------+  +--------------+ | |
|                                   |                           |   |
|   +-------------------------------+---------------------------+   |
|   |                                                           |   |
|   v                                                           v   |
| +------------------------------------+  +-----------------------+ |
| | 5. Replicator (Synchronous Quorum) |  | 6. Gossip Protocol    | |
| |    - Replicates to N-1 Peer Nodes  |  |    - Heartbeat (2s)   | |
| |    - Verifies Write Quorum (W=2)   |  |    - Auto Discovery  | |
| +------------------------------------+  +-----------------------+ |
+-------------------------------------------------------------------+
```

---

## 🔄 Deep Dive: Step-by-Step Lifecycle of `PUT("user:123", "Tejas")`

---

### Step 1: Ingress & Network Gateway (`Router.cpp`)
1. **Request Reception:** A client issues an HTTP POST request to `/put` with payload `key="user:123"&value="Tejas"` to any arbitrary node in the cluster (Node A).
2. **CORS & Preflight Handling:** The router pre-routing handler evaluates OPTIONS preflight requests, adding headers `Access-Control-Allow-Origin: *` and `X-Pinggy-No-Screen: true` to prevent browser blockages.
3. **Consensus Config Read:** The server acquires a shared lock (`std::shared_lock<std::shared_mutex>`) on global configuration settings to retrieve the active parameters: $N=3$ (Replication Factor) and $W=2$ (Write Quorum requirement).

---

### Step 2: Hashing & Topology Lookup (`ConsistentHash.cpp`)
1. **Hash Calculation:** The server passes the key `"user:123"` into the **64-bit FNV-1a Hashing Algorithm**:
   $$\text{hash} = \text{FNV\_offset\_basis} \bigoplus \text{char} \times \text{FNV\_prime}$$
2. **vNode Ring Lookup:** The ring maintains 100 virtual nodes per physical machine stored in a binary search tree (`std::map<size_t, std::string>`).
3. **Clockwise Routing:** The engine invokes `std::map::upper_bound(hash)`. It traverses clockwise to find the first vNode whose token exceeds the key's hash.
4. **Proxy Decision:**
   * **If Node A is NOT the owner:** Node A initiates an HTTP client call (`httplib::Client`) to proxy the request to the owner node (Node B).
   * **If Node A IS the owner:** Node A assumes the role of the **Write Coordinator** for this transaction.

---

### Step 3: Local Disk Durability — Write-Ahead Logging (`WAL.cpp`)
Before modifying RAM, the coordinator guarantees crash resilience:
1. **File Lock:** Takes an exclusive thread lock (`std::lock_guard<std::mutex>`).
2. **Format & Append:** Formats the record as `"PUT,user:123,Tejas\n"` and appends it to the disk log file (`data/wal_<port>.log`).
3. **Forced Disk Sync:** Executes `logFile.flush()`. This bypasses the OS buffer cache, forcing data directly to persistent disk storage.
   * *Interview Note:* If power fails after this millisecond, node recovery will replay the log on boot (`WAL::recover`) to fully restore the state in RAM.

---

### Step 4: In-Memory Storage Mutation & LRU Eviction (`StorageEngine.cpp`)
1. **Exclusive Lock:** The engine acquires an exclusive Write Lock (`std::unique_lock<std::shared_mutex>`). This blocks concurrent writers while guaranteeing thread safety.
2. **Capacity & Eviction Check:** If key count hits max capacity (10,000 keys):
   * Evicts the tail of the doubly-linked LRU list (`std::list<std::string> lruQueue`).
   * Erases the key from `lruMap` and main `dataStore` (`std::unordered_map`).
3. **Insertion & LRU Re-ordering:**
   * Stores `{value: "Tejas", expiry_time: 0}` in `dataStore`.
   * Promotes `"user:123"` to the front of `lruQueue` and updates `lruMap` iterator pointer in $O(1)$ time.
4. **Local ACK:** Coordinator sets its internal acknowledgment counter `acks = 1`.

---

### Step 5: Synchronous Replication & Peer Quorum (`Replicator.cpp`)
1. **Target Selection:** The coordinator retrieves unique physical nodes from the ring, filtering out itself, and targets $N-1$ replica nodes.
2. **Synchronous Forwarding:** Sends HTTP POST `/internal/replicate` calls with a strict 2-second connection/read timeout (`cli.set_read_timeout(2, 0)`).
3. **Replica Node Processing:** Each receiving replica node:
   * Appends `"PUT,user:123,Tejas"` to its local WAL disk file.
   * Mutates its local in-memory `StorageEngine`.
   * Responds with `200 OK`.
4. **ACK Accumulation:** For each `200 OK` response received within timeout, coordinator increments `acks++`.

---

### Step 6: Quorum Verification & Rollback Handling (`Router.cpp`)
1. **Quorum Evaluation:** Checks if total ACKs satisfy the effective Write Quorum:
   $$\text{acks} \ge \text{effective\_W} \quad \text{where } \text{effective\_W} = \min(W, \text{activeNodes})$$
2. **Scenario A — SUCCESS ($\text{acks} \ge 2$):**
   * Coordinator responds with HTTP `200 OK` and JSON `{"status": "success"}`.
   * Client receives confirmation that the write is durable and replicated.
3. **Scenario B — FAILURE / ROLLBACK ($\text{acks} < 2$):**
   * If network partition or peer crash prevents reaching quorum:
   * **Memory Rollback:** Calls `storage->remove("user:123")` to clear the uncommitted key.
   * **WAL Tombstone:** Writes `"DELETE,user:123,"` tombstone to WAL to maintain disk consistency.
   * **Error Response:** Returns HTTP `503 Service Unavailable` with `{"error": true, "message": "Write Quorum Failed"}`.

---

### Step 7: Background Async Protocols (`Gossip.cpp` & Garbage Collection)
* **Gossip Failure Detection:** Every 2 seconds, a background OS thread (`std::thread`) sends `/ping` heartbeats to peer nodes. If 3 consecutive heartbeats fail, the node is marked DEAD, evicted from the hash ring, and a dynamic cluster rebalance is triggered.
* **TTL Garbage Collector:** A dedicated background thread wakes every 1 second to purge expired keys from RAM and LRU structures.

---

## 💡 Quick Reference: Component Mapping Table

| Layer / Concept | Code Implementation File | Data Structures / Tech Used |
| :--- | :--- | :--- |
| **Networking & Gateway** | [Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp) | `cpp-httplib`, HTTP REST, CORS headers |
| **Consistent Hashing** | [ConsistentHash.cpp](file:///home/devian/Projects/distributed-system/src/core/ConsistentHash.cpp) | FNV-1a 64-bit Hash, 100 vNodes, `std::map` (BST), `std::shared_mutex` |
| **Durability (WAL)** | [WAL.cpp](file:///home/devian/Projects/distributed-system/src/core/WAL.cpp) | Append-only file streams (`std::ofstream`), `logFile.flush()`, `std::mutex` |
| **Storage Engine & LRU** | [StorageEngine.cpp](file:///home/devian/Projects/distributed-system/src/core/StorageEngine.cpp) | `std::unordered_map`, `std::list` (LRU), `std::shared_mutex` (RW Lock) |
| **Replication & Quorum** | [Replicator.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Replicator.cpp) | Synchronous HTTP calls, 2s Timeout, $W=2, R=2, N=3$ Quorum |
| **Failure Detection** | [Gossip.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Gossip.cpp) | OS-level Background Thread (`std::thread`), 2s `/ping` Heartbeats |
