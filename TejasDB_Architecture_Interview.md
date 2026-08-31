# 🎤 Distributed Key-Value Store: Interview Response

> **Obsidian Note**  
> **Tags:** `#distributed-systems` `#interview-prep` `#cpp` `#system-design`  
> **Project:** Tejas-DB  

---

## ❓ Question
**"Explain the complete architecture of your distributed key-value database. Start from the moment a client sends `PUT("user:123", "Tejas")`. Walk me through exactly what happens—from receiving the request until the write is considered successful."**

---

## 🗣️ Concise Candidate Answer

"Sure! **Tejas-DB** is a masterless distributed key-value store built in C++17, inspired by Amazon Dynamo. It has no single point of failure and uses Consistent Hashing, Write-Ahead Logging (WAL), and Strict Quorum Consensus ($W=2, R=2, N=3$).

Here is the step-by-step lifecycle of `PUT("user:123", "Tejas")`:

### 1. Ingress & Routing
* The client sends a `POST /put` request to any node in the cluster over HTTP.
* The receiving node acts as the **Coordinator**. It computes the key's hash using **64-bit FNV-1a** and checks our **Consistent Hash Ring** (which uses 100 vNodes per server for uniform distribution).
* If the receiving node isn't the owner, it proxies the request to the owner node. If it *is* the owner, it coordinates the write.

### 2. Local Durability (WAL)
* Before writing to memory, the coordinator writes `PUT,user:123,Tejas` to a disk-backed **Write-Ahead Log (WAL)** using `std::lock_guard<std::mutex>`.
* It calls `flush()` immediately to ensure disk durability before touching RAM.

### 3. Memory Mutation (LRU Cache)
* Under an exclusive write lock (`std::unique_lock<std::shared_mutex>`), the value is stored in our in-memory `unordered_map`.
* If capacity is full, the least recently used key is evicted via an LRU double-linked list (`std::list`).
* Local ACK count becomes **1**.

### 4. Synchronous Quorum Replication
* The coordinator looks up the active physical nodes on the ring and sends synchronous HTTP `/internal/replicate` calls to replica nodes.
* Each replica node appends to its local WAL and writes to its RAM, returning `200 OK`.
* Every successful replica ACK increments our count (`acks++`).

### 5. Consensus & Acknowledgment
* We evaluate write quorum: **$acks \ge W$** (where $W=2$).
* **Success:** If quorum is met, the coordinator returns `200 OK` `{"status": "success"}` to the client.
* **Failure:** If quorum fails (e.g. network partition/node failures), the coordinator rolls back the local memory write, logs a tombstone, and returns `503 Service Unavailable`.

In the background, a **Gossip Protocol** thread pings peers every 2s for failure detection, and a background cleanup thread handles TTL evictions."
