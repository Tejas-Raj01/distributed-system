#!/usr/bin/env bash

# Project directory path
PROJECT_DIR="/home/devian/Projects/distributed-system"

cd "$PROJECT_DIR" || exit 1

# Make sure backend is compiled
if [ ! -f "build/kv_server" ]; then
    mkdir -p build && cd build && cmake .. && make && cd ..
fi

mkdir -p data

# Stop old instances if running
pkill -f 'kv_server' 2>/dev/null || true
pkill -f 'vite --host' 2>/dev/null || true

# Start 3 C++ Backend Nodes (8080, 8082, 8084)
nohup ./build/kv_server 8080 127.0.0.1:8082 127.0.0.1:8084 > data/backend_8080.log 2>&1 &
nohup ./build/kv_server 8082 127.0.0.1:8080 127.0.0.1:8084 > data/backend_8082.log 2>&1 &
nohup ./build/kv_server 8084 127.0.0.1:8080 127.0.0.1:8082 > data/backend_8084.log 2>&1 &

# Start React/Vite Frontend
cd "$PROJECT_DIR/frontend" || exit 1
nohup npm run dev > "$PROJECT_DIR/data/frontend.log" 2>&1 &

echo "🚀 Tejas-DB 3-Node Backend Cluster & Frontend started in background!"
