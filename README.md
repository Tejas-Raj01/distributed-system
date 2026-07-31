# 🚀 Tejas-DB: High-Performance Distributed Key-Value Store

![C++](https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=c%2B%2B)
![React](https://img.shields.io/badge/React-UI-61DAFB.svg?style=for-the-badge&logo=react)
![Architecture](https://img.shields.io/badge/Architecture-Distributed-success.svg?style=for-the-badge)
![Throughput](https://img.shields.io/badge/Throughput-33k%2B%20RPS-orange.svg?style=for-the-badge)

A highly concurrent, fault-tolerant, and masterless distributed Key-Value store built entirely from scratch in **Modern C++**, featuring a real-time visual dashboard built in **React & Tailwind CSS**. 

Engineered to handle massive scalability, this system implements core distributed systems concepts inspired by Amazon Dynamo and Apache Cassandra.

---

## ✨ Core Architecture & Features

### 1. 🔄 Consistent Hashing & Data Distribution
* Implemented a **Ring Topology** using the **64-bit FNV-1a Hash Algorithm** to ensure uniform data distribution and eliminate hot spots.
* Configured with **100 Virtual Nodes (vNodes)** per physical server to achieve perfect load balancing and seamless scaling.

### 2. 🛡️ Fault Tolerance & Replication
* **Strict Quorum Logic (W=2, R=2):** Ensures consistency across cluster nodes. Writes are acknowledged once replica quorums are achieved.
* **Write-Ahead Logging (WAL):** Disk-backed crash recovery. Every node recovers its exact state from local logs instantly upon reboot.
* **Gossip Protocol:** Background heartbeat threads monitor cluster health, instantly detecting node failures and dynamically updating the routing topology.

### 3. ⚡ High-Performance Concurrency
* Architected a thread-safe custom Storage Engine utilizing `std::shared_mutex` (Read-Write Locks).
* Achieved **33,685 Write Requests Per Second (RPS)** with a highly concurrent load of 100 simultaneous threads, maintaining an average latency of just **~2.97ms** (Benchmarked via Apache `ab`).

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
* CMake 3.10+
* Apache Bench (`ab` tool for load testing)
* Node.js & npm (For the UI)

### 1. Build the Backend (Release Mode with -O3 Optimizations)
```bash
# Clone the repository
git clone https://github.com/Tejas-Raj01/distributed-system.git
cd distributed-system

# Create release build with maximum optimizations
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3"
cmake --build build
```

### 2. Start the Backend Server
```bash
# Ensure data directory exists for WAL logging
mkdir -p data

# Start a server node on port 8080
./build/kv_server 8080
```

---

## ⚡ Performance Benchmarking & Load Testing

To run high-concurrency performance benchmarks matching our test setup (100 concurrent threads, 50,000 requests), follow these steps:

### Step 1: Install Apache Benchmark (`ab`)
- **Ubuntu/Debian:** `sudo apt install apache2-utils`
- **Fedora/RHEL:** `sudo dnf install httpd-tools`
- **Arch Linux:** `sudo pacman -S apache`

### Step 2: Start Server in Background
```bash
./build/kv_server 8080 > data/server.log 2>&1 &
```

### Step 3: Write (POST) Benchmark Execution
1. Create a post payload file:
   ```bash
   echo "key=StressTestKey&value=MassiveDataLoad" > payload.txt
   ```
2. Run Apache Bench for 50,000 POST requests with 100 concurrent connections:
   ```bash
   ab -n 50000 -c 100 -p payload.txt -T "application/x-www-form-urlencoded" "http://127.0.0.1:8080/put"
   ```

### Step 4: Read (GET) Benchmark Execution
Run Apache Bench for 50,000 GET requests under 100 concurrent connections:
```bash
ab -n 50000 -c 100 "http://127.0.0.1:8080/get?key=StressTestKey"
```

---

## 📊 Benchmark Results & Detailed Analytics

Tested on Release build (`-O3` optimized) with **50,000 requests** under **100 concurrent threads**:

| Metric / Parameter | Write (POST `/put`) | Read (GET `/get`) |
| :--- | :--- | :--- |
| **Total Requests** | 50,000 | 50,000 |
| **Concurrency Level** | 100 connections | 100 connections |
| **Throughput (Requests/sec)** | **33,684.94 req/sec** | **1,809.14 req/sec** |
| **Mean Latency (Average)** | **2.969 ms** | **55.275 ms** |
| **Concurrent Request Latency** | **0.030 ms** | **0.553 ms** |
| **Success / Error Rate** | **100% Success (0 Errors)** | **100% Success (0 Errors)** |

### Latency Percentile Breakdown

| Percentile | Write Latency (ms) | Read Latency (ms) |
| :--- | :--- | :--- |
| **50% (Median)** | **1 ms** | **49 ms** |
| **66%** | **2 ms** | **50 ms** |
| **75%** | **2 ms** | **50 ms** |
| **80%** | **2 ms** | **51 ms** |
| **90%** | **3 ms** | **71 ms** |
| **95%** | **4 ms** | **72 ms** |
| **98%** | **5 ms** | **73 ms** |
| **99%** | **5 ms** | **73 ms** |