#!/usr/bin/env bash

# Project directory path
PROJECT_DIR="/home/devian/Projects/distributed-system"
cd "$PROJECT_DIR" || exit 1

echo "🚀 Restarting Backend Servers..."
./scripts/start_server.sh

echo "🔄 Stopping old Pinggy tunnels..."
pkill -f 'ssh.*pinggy' 2>/dev/null || true

echo "🌐 Starting new Pinggy Tunnel in background..."
nohup ssh -p 443 -R0:localhost:8080 -o StrictHostKeyChecking=accept-new a.pinggy.io > data/pinggy.log 2>&1 &

# Wait for the tunnel URL to appear in the log
echo "⏳ Waiting for Pinggy URL..."
sleep 5
NEW_URL=$(grep -oE "https://[a-zA-Z0-9-]+\.free\.pinggy\.net" data/pinggy.log | head -n 1)

if [ -z "$NEW_URL" ]; then
    echo "❌ Failed to get Pinggy URL! (Maybe free limit reached or connection issue)"
    cat data/pinggy.log
    exit 1
fi

echo "✅ New Tunnel URL: $NEW_URL"

echo "📝 Updating Vercel configuration files..."
# Use sed to replace any old pinggy URL with the new one
sed -i -E "s|https://[a-zA-Z0-9-]+\.free\.pinggy\.net|$NEW_URL|g" vercel.json
sed -i -E "s|https://[a-zA-Z0-9-]+\.free\.pinggy\.net|$NEW_URL|g" frontend/vercel.json

echo "🚀 Committing and pushing to trigger Vercel deployment..."
git add vercel.json frontend/vercel.json
git commit -m "chore: auto-update pinggy tunnel URL"
git push origin main

echo "🎉 Done! The Vercel app will be live with the new backend URL in ~1 minute."
