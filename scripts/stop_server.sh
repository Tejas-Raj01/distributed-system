#!/usr/bin/env bash

pkill -f 'kv_server' 2>/dev/null || true
pkill -f 'vite --host' 2>/dev/null || true

echo "🛑 Tejas-DB Backend & Frontend stopped!"
