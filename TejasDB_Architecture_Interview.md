# 🎤 Distributed Key-Value Store: Interview Response Guide

> **Obsidian Note**  
> **Tags:** `#distributed-systems` `#interview-prep` `#cpp` `#system-design` `#architecture`  
> **Project:** Tejas-DB  

---

## ❓ Question 1: Architecture & End-to-End Request Lifecycle
**"Suppose I know nothing about your project. Explain the complete architecture of your distributed key-value database. Start from the moment a client sends `PUT("user:123", "Tejas")`. Walk me through exactly what happens—from receiving the request until the write is considered successful."**

---

## 🎙️ Executive Summary Answer (30-Second Elevator Pitch)

> "Tejas-DB is a **masterless, peer-to-peer distributed Key-Value store** built in C++17 inspired by Amazon Dynamo. It eliminates single points of failure using **Consistent Hashing** (64-bit FNV-1a algorithm with 100 virtual nodes per server), guarantees durability via **Write-Ahead Logging (WAL)**, delivers high-performance concurrency with a **thread-safe LRU Storage Engine** using reader-writer locks (`std::shared_mutex`), and enforces consistency using **Strict Quorum Consensus** (W=2, R=2, N=3).
>
> When a write request `PUT("user:123", "Tejas")` arrives, any node can act as the **Coordinator**. It computes the hash, routes the key to the responsible owner node, writes to disk-backed WAL first, mutates memory, synchronizes replicas over HTTP, evaluates write quorum (W=2), and returns a response to the client—rolling back if quorum fails."

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
3. **Consensus Config Read:** The server acquires a shared lock (`std::shared_lock<std::shared_mutex>`) on global configuration settings to retrieve the active parameters: N=3 (Replication Factor) and W=2 (Write Quorum requirement).

---

### Step 2: Hashing & Topology Lookup (`ConsistentHash.cpp`)
1. **Hash Calculation:** The server passes the key `"user:123"` into the **64-bit FNV-1a Hashing Algorithm**:
   `hash = FNV_offset_basis XOR char * FNV_prime`
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
   * Promotes `"user:123"` to the front of `lruQueue` and updates `lruMap` iterator pointer in O(1) time.
4. **Local ACK:** Coordinator sets its internal acknowledgment counter `acks = 1`.

---

### Step 5: Synchronous Replication & Peer Quorum (`Replicator.cpp`)
1. **Target Selection:** The coordinator retrieves unique physical nodes from the ring, filtering out itself, and targets N-1 replica nodes.
2. **Synchronous Forwarding:** Sends HTTP POST `/internal/replicate` calls with a strict 2-second connection/read timeout (`cli.set_read_timeout(2, 0)`).
3. **Replica Node Processing:** Each receiving replica node:
   * Appends `"PUT,user:123,Tejas"` to its local WAL disk file.
   * Mutates its local in-memory `StorageEngine`.
   * Responds with `200 OK`.
4. **ACK Accumulation:** For each `200 OK` response received within timeout, coordinator increments `acks++`.

---

### Step 6: Quorum Verification & Rollback Handling (`Router.cpp`)
1. **Quorum Evaluation:** Checks if total ACKs satisfy the effective Write Quorum:
   `acks >= effective_W` where `effective_W = min(W, activeNodes)`
2. **Scenario A — SUCCESS (acks >= 2):**
   * Coordinator responds with HTTP `200 OK` and JSON `{"status": "success"}`.
   * Client receives confirmation that the write is durable and replicated.
3. **Scenario B — FAILURE / ROLLBACK (acks < 2):**
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
| **Replication & Quorum** | [Replicator.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Replicator.cpp) | Synchronous HTTP calls, 2s Timeout, W=2, R=2, N=3 Quorum |
| **Failure Detection** | [Gossip.cpp](file:///home/devian/Projects/distributed-system/src/cluster/Gossip.cpp) | OS-level Background Thread (`std::thread`), 2s `/ping` Heartbeats |

---
---

## ❓ Question 2: Concurrent Writes & Conflict Resolution

**"Two clients simultaneously send these requests:**
* **Client A:** `PUT("user:123", "Tejas")`
* **Client B:** `PUT("user:123", "Raj")`

**Assume both requests arrive at different nodes at almost exactly the same time. How does your system handle these concurrent writes? Explain:**
1. **Which node coordinates each request?**
2. **How both writes are replicated.**
3. **What happens if different replicas receive them in different orders.**
4. **How your database decides which value is the latest.**
5. **What a subsequent `GET("user:123")` returns.**

**Answer based on your actual implementation."**

---

## 🎙️ Candidate Executive Summary (30-Second Elevator Pitch)

> "In Tejas-DB, concurrent writes to the same key are funneled through a **single, deterministic Owner Node** determined by our Consistent Hash ring. Even if Client A hits Node 1 and Client B hits Node 2 simultaneously, both nodes calculate the exact same 64-bit FNV-1a hash for `"user:123"` and proxy their requests to the designated Owner Node (say Node 3).
>
> Node 3 acts as the single Write Coordinator. It **serializes both requests** using an in-memory C++ write lock (`std::unique_lock<std::shared_mutex>`). Whichever request acquires the lock first writes to WAL and memory, and triggers synchronous replication to peers. The second write then acquires the lock and overwrites the first. Because replication is sequential from this single coordinator, all replicas receive the writes in identical order. A subsequent `GET("user:123")` will deterministically return whichever value won the mutex race at the coordinator."

---

## 🎯 1. Which Node Coordinates Each Request? (Single-Owner Determinism)

### **The Mechanism: Consistent Hashing Ownership**
* **Client A** sends `PUT("user:123", "Tejas")` to **Node 1**.
* **Client B** sends `PUT("user:123", "Raj")` to **Node 2**.
* **Hash Calculation:** Both Node 1 and Node 2 independently execute:
  `hash = ConsistentHash::computeHash("user:123")`
* **Ring Lookup:** Both nodes call `hashRing->getOwnerNode("user:123")`. Because 64-bit FNV-1a hashing on a static ring topology is 100% deterministic, both nodes find the exact same primary owner node—let's call it **Node 3**.
* **Request Proxying:**
  * Node 1 detects `ownerNode (Node 3) != myAddress (Node 1)`. It proxies Client A's request to Node 3 via HTTP `Post("/put")`.
  * Node 2 detects `ownerNode (Node 3) != myAddress (Node 2)`. It proxies Client B's request to Node 3 via HTTP `Post("/put")`.

> **Key takeaway for interviewer:** We avoid multi-master concurrent write conflicts for a single key because **all writes for `"user:123"` are funneled to a single designated Coordinator Node (Node 3)**.

---

## 🔁 2. How Both Writes Are Replicated (Coordinator Mutex Serialization)

### **Thread Synchronization at Node 3**
Node 3 receives Client A's proxied POST request and Client B's proxied POST request concurrently on two separate HTTP server threads in `cpp-httplib`.

1. **Mutex Acquisition Race:**
   * Both threads enter `Router::handlePut()`.
   * Both threads call `StorageEngine::put()`, which executes:
     ```cpp
     std::unique_lock<std::shared_mutex> lock(rw_lock); // Write Lock
     ```
   * Only **one** thread can hold `rw_lock` at a time. Suppose Client A's thread wins the race and acquires the lock first.

2. **Sequential Write Execution:**
   * **Client A's Execution (Wins Lock First):**
     1. Appends `"PUT,user:123,Tejas"` to WAL (`wal->appendLog()`) under `file_lock` and calls `flush()`.
     2. Updates local memory: `dataStore["user:123"] = {value: "Tejas", expiry: 0}`.
     3. Releases `rw_lock`.
     4. Triggers synchronous `/internal/replicate` HTTP calls to replica nodes (Node 1 and Node 2) via `Replicator::forwardToReplica()`.
   * **Client B's Execution (Queued Behind Client A):**
     1. Unblocks as soon as Client A's write finishes.
     2. Appends `"PUT,user:123,Raj"` to WAL (`wal->appendLog()`) and calls `flush()`.
     3. Overwrites local memory: `dataStore["user:123"] = {value: "Raj", expiry: 0}`.
     4. Releases `rw_lock`.
     5. Triggers synchronous `/internal/replicate` HTTP calls to replica nodes (Node 1 and Node 2).

---

## 🔀 3 & 4. Replica Order, State Resolution & Last-Write-Wins (LWW)

### **Q3: What if replicas receive them in different orders?**
In our actual implementation, because replication calls (`/internal/replicate`) are sent **synchronously and sequentially by the single coordinator (Node 3)**:
* Node 3 finishes replicating Client A's payload to Node 1 and Node 2.
* Node 3 then replicates Client B's payload to Node 1 and Node 2.
* Therefore, replica nodes receive `/internal/replicate` calls in the **exact same arrival sequence as Node 3 processed them**.

### **Q4: How does the database decide which value is the latest?**
* **Actual Implementation Detail:** Our current engine uses **Coordinator Ingress Race Order (Mutex LWW)**.
* When `StorageEngine::put()` is invoked on Node 3 or any replica node, `dataStore["user:123"]` is updated via:
  ```cpp
  dataStore[key] = {value, expiry};
  ```
* Whichever request acquired the coordinator's mutex last (`"Raj"` in our example) overwrites `"Tejas"` in `std::unordered_map`.
* **Honest Architectural Note:** We do *not* currently store physical wall-clock timestamps (`uint64_t timestamp_ms`) or vector clocks inside the `Record` struct. The "latest" value is defined by the **lock acquisition sequence at the Owner Node**.

---

## 📖 5. What a Subsequent `GET("user:123")` Returns

1. A client issues `GET("user:123")` to any node in the cluster.
2. The receiving node calculates `getOwnerNode("user:123")` and routes/proxies the request to **Node 3** (the owner).
3. Node 3 executes `StorageEngine::get("user:123")` under a read lock (`std::unique_lock<std::shared_mutex>`).
4. **Return Value:** It returns **`"Raj"`** (assuming Client B won the mutex race on Node 3).
5. **Consistency Guarantee:** Because all GET and PUT requests for `"user:123"` route to Node 3, any subsequent read immediately reflects `"Raj"`, achieving **Read-Your-Writes and Sequential Consistency** for that key.

---

## 🛠️ Honest Engineering Analysis & Production Enhancement Path

If an interviewer asks: *"What if network delays cause replica nodes to receive async replication in reverse order?"*

> **Candidate Response:**  
> *"That's a great question! In our current v1 implementation, replication is synchronous and coordinated by a single owner node, which prevents out-of-order delivery.
> 
> However, to make the cluster resilient against asynchronous network delays or multi-leader setups, our next architectural upgrade is to add **Last-Write-Wins (LWW) Timestamps**:
> 1. Extend `Record` struct in `StorageEngine.hpp` to include `uint64_t timestamp_ms`.
> 2. Inside `StorageEngine::put()`, check `if (incoming_timestamp >= existing_timestamp)`, only then overwrite `dataStore[key]`.
> 3. If `incoming_timestamp < existing_timestamp`, drop the stale replica write.
> 
> This would enforce LWW determinism even under asynchronous network reordering."*

---
---

## ❓ Question 3: Consistency Model, Read Path & Quorum Realities (N=3, W=2, R=2)

**"Tejas, I want to understand your consistency model better.**

**You said your replication configuration is N=3, W=2, R=2. But you also just told me that every GET request is routed to the owner node.**

**So where exactly is the R=2 read quorum used?**

**Walk me through `GET("user:123")` step by step.**

**Specifically explain:**
1. **Does the owner query two replicas?**
2. **Or does it only read from its local memory?**
3. **If it queries two replicas, how does it resolve different values?**
4. **If it only reads locally, in what sense is R=2 implemented?**
5. **What happens if the owner node is down?**

**Answer based strictly on your actual implementation."**

---

## 🎙️ Candidate Executive Summary (30-Second Elevator Pitch)

> "In our actual C++ implementation of Tejas-DB, every `GET("user:123")` request is routed to the key's primary Owner Node via Consistent Hashing. Once the request hits the owner node, **the owner node reads directly from its local in-memory storage engine rather than making network calls to query two replicas.**
>
> In our current codebase, `R=2` is implemented as a **configurable consistency tier parameter** that models read latency semantics (`std::this_thread::sleep_for(current_R * 12)` ms). Local reading works safely because writes require synchronous quorum (`W=2`), guaranteeing that the owner node holds the latest committed state (`W + R > N` => `2 + 2 > 3`).
>
> If the primary owner node crashes, our **Gossip Failure Detector** detects the failure, evicts the dead node from the Consistent Hash ring, and promotes the next clockwise replica node in the ring—which already received the `W=2` synchronous write—to serve reads."

---

## 🔄 Step-by-Step Execution Lifecycle of `GET("user:123")`

When a client issues an HTTP `GET /get?key=user:123`:

1. **Ingress & Parameter Extraction (`Router.cpp`):**
   * The node receiving the request extracts `key = "user:123"`.
   * It acquires a shared lock on `global_config` to read the active read quorum setting `current_R`.
2. **Configured Latency Simulation:**
   * If `current_R == 1`, it executes `sleep_for(2ms)`.
   * If `current_R > 1` (e.g., `R=2`), it executes `sleep_for(current_R * 12ms)` (24ms) to simulate quorum evaluation overhead.
3. **Consistent Hash Routing (`ConsistentHash.cpp`):**
   * Evaluates `hashRing->getOwnerNode("user:123")`.
4. **Local Read vs Proxy Branching:**
   * **If Receiving Node IS the Owner Node:** Calls `storage->get("user:123")` under `std::shared_lock<std::shared_mutex>` and returns HTTP `200 OK` with the value.
   * **If Receiving Node IS NOT the Owner Node:** Instantiates an HTTP client (`httplib::Client`) to proxy the GET request to `http://<ownerNode>/get?key=user:123`.

---

## 🎯 Direct Answers to the 5 Interview Sub-Questions

### 1. Does the owner query two replicas?
**No.** In [Router.cpp](file:///home/devian/Projects/distributed-system/src/network/Router.cpp#L237-L245), when `ownerNode == myAddress`, the server executes:
```cpp
auto result = storage->get(key);
```
It does not initiate outbound HTTP or RPC calls to fan out reads to `R-1` replica nodes.

### 2. Or does it only read from its local memory?
**Yes.** The owner node acquires a shared Reader Lock (`std::shared_lock<std::shared_mutex>`) on its local `StorageEngine` in-memory `std::unordered_map` and returns the local value directly.

### 3. If it queries two replicas, how does it resolve different values?
Because the owner node reads locally, **it does not perform dynamic multi-node read resolution (Read Repair) on the read path.** State resolution occurs on the **write path**: writes are synchronously replicated to `W=2` nodes, and single-owner mutex lock serialization ensures all replicas receive writes in identical order.

### 4. If it only reads locally, in what sense is R=2 implemented?
In our codebase:
* **Latency Simulation:** `R` acts as a configurable parameter (`global_config.R`) that dynamically throttles read response latency (`sleep_for(current_R * 12)` ms) to simulate strict quorum consensus network delays.
* **Theoretical Guarantee:** Because writes enforce `W=2` across `N=3` nodes, the owner node is guaranteed to be part of every successful write quorum. Therefore, local reads from the owner node implicitly satisfy strong consistency (`W + R > N`) without the performance penalty of `R` parallel network reads.

### 5. What happens if the owner node is down?
* **Immediate Proxy Failure:** If a non-owner receives a `GET` request and the owner node is unreachable, the proxy HTTP request fails and returns HTTP `503 Service Unavailable` ("Owner node is unreachable").
* **Gossip Eviction & Ring Failover:** Within 6 seconds (3 failed heartbeats at 2s intervals), the `Gossip` thread marks the owner node as DEAD and evicts it from the ring.
* **Replica Promotion:** The next node clockwise on the vNode ring becomes the new primary owner. Because `W=2` writes were synchronously replicated to this replica, the new owner immediately serves the latest value from its local memory/WAL without data loss.

---

## 🛠️ Honest Engineering Analysis & Production Enhancement Path

If the interviewer pushes: *"So R=2 isn't a true multi-node RPC read quorum in code yet?"*

> **Candidate Response:**  
> *"Spot on! That is an accurate distinction between our v1 implementation and a full Dynamo specification.
> 
> In v1, we optimized the read path by routing reads to the single owner node, which avoids fan-out network latency while relying on `W=2` synchronous writes for consistency.
> 
> To upgrade this to **True Fan-Out Quorum Reads (R=2 Read Repair)**:
> 1. In `Router::handleGet()`, the coordinator node sends parallel HTTP GET calls to `R` top nodes on the ring.
> 2. Attach a monotonic timestamp or vector version to each record.
> 3. Compare returned values: if replicas return conflicting versions, return the latest timestamp to the client and asynchronously send a `/internal/put` write to update the stale replica (**Read Repair**)."*

