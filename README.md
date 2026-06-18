# 🚀 Tejas-DB: High-Performance Distributed Key-Value Store

![C++](https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=c%2B%2B)
![React](https://img.shields.io/badge/React-UI-61DAFB.svg?style=for-the-badge&logo=react)
![Architecture](https://img.shields.io/badge/Architecture-Distributed-success.svg?style=for-the-badge)
![Throughput](https://img.shields.io/badge/Throughput-16k%20RPS-orange.svg?style=for-the-badge)

A highly concurrent, fault-tolerant, and masterless distributed Key-Value store built entirely from scratch in **Modern C++**, featuring a real-time visual dashboard built in **React & Tailwind CSS**. 

Engineered to handle massive scalability, this system implements core distributed systems concepts inspired by Amazon Dynamo and Apache Cassandra.

---

## ✨ Core Architecture & Features

### 1. 🔄 Consistent Hashing & Data Distribution
* Implemented a **Ring Topology** using the **64-bit FNV-1a Hash Algorithm** to ensure uniform data distribution and eliminate hot spots.
* Configured with **100 Virtual Nodes (vNodes)** per physical server to achieve perfect load balancing and seamless scaling.

### 2. 🛡️ Fault Tolerance & Replication
* **Strict Quorum Logic (W=2):** Ensures strong consistency. A write operation is only considered successful if acknowledged by at least 2 nodes. Fails safely with automatic rollback.
* **Write-Ahead Logging (WAL):** Disk-backed crash recovery. Every node recovers its exact state from local logs instantly upon reboot.
* **Gossip Protocol:** Background heartbeat threads monitor cluster health, instantly detecting node failures and dynamically updating the routing topology.

### 3. ⚡ High-Performance Concurrency
* Architected a thread-safe custom Storage Engine utilizing `std::shared_mutex` (Read-Write Locks).
* Achieved **16,132 Requests Per Second (RPS)** with a highly concurrent load of 100 simultaneous threads, maintaining an average latency of just **~6ms** (Benchmarked via Apache `ab`).

### 4. ⚖️ Elastic Scaling & Live Rebalancing
* Dynamically supports adding or removing nodes.
* Exposes an `/admin/rebalance` endpoint that intelligently calculates hash boundaries and migrates live data to new nodes without cluster downtime.

### 5. 🖥️ Interactive Visual Dashboard (Control Center)
* A real-time **React/Vite** dashboard that visualizes the Consistent Hashing Ring.
* **Chaos Engineering Controls:** Inject massive workloads or simulate node deaths with a single click to visually demonstrate data rebalancing and fault tolerance in real-time.

---

## 🛠️ Tech Stack

* **Backend Engine:** Modern C++ (C++17)
* **Networking/HTTP:** `cpp-httplib`
* **Frontend UI:** React.js, Vite, Tailwind CSS
* **Performance Testing:** Apache Bench (`ab`) & Custom Bash Scripts

---

## 🚀 Getting Started

### Prerequisites
* GCC/G++ (Supports C++17)
* CMake & Make
* Node.js & npm (For the UI)

### 1. Build the Backend
```bash
mkdir build && cd build
cmake ..
make