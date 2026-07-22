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
pkill -f 'kv_server 8080' 2>/dev/null || true
pkill -f 'vite --host' 2>/dev/null || true

# Start C++ Backend
./build/kv_server 8080 > data/backend.log 2>&1 &

# Start React/Vite Frontend
cd "$PROJECT_DIR/frontend" || exit 1
npm run dev > "$PROJECT_DIR/data/frontend.log" 2>&1 &

echo "🚀 Tejas-DB Backend & Frontend started in background!"
