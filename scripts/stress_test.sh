#!/usr/bin/env bash

# Colors for terminal output
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${GREEN}==========================================${NC}"
echo -e "${GREEN}  KV-Store Load & Performance Test Suite  ${NC}"
echo -e "${GREEN}==========================================${NC}"

# Check if Apache Bench (ab) is installed
if ! command -v ab &> /dev/null
then
    echo "Error: Apache Bench (ab) tool is not installed."
    echo "NixOS pe install karne ke liye run karein: nix-shell -p apacheHttpd"
    exit 1
fi

# Variables
TOTAL_REQUESTS=10000
CONCURRENCY=100
# Hum '/internal/put' use kar rahe hain taaki pure raw speed (Storage + RAM) test ho sake
# bina Quorum/Replica network latency ke.
URL="http://127.0.0.1:8080/internal/put" 
PAYLOAD_FILE="scripts/post_data.txt"
CONTENT_TYPE="application/x-www-form-urlencoded"

echo -e "${CYAN}Running test with $TOTAL_REQUESTS requests and $CONCURRENCY concurrent users...${NC}"
echo -e "Target URL: $URL"
echo "------------------------------------------"

# Run Apache Bench
ab -n $TOTAL_REQUESTS -c $CONCURRENCY -p $PAYLOAD_FILE -T $CONTENT_TYPE $URL

echo -e "${GREEN}==========================================${NC}"
echo -e "${GREEN} Test Completed! Check 'Requests per second' ${NC}"
echo -e "${GREEN}==========================================${NC}"