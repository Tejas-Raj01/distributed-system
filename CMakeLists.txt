cmake_minimum_required(VERSION 3.10)
project(KVStoreServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)

# Include directories for headers
include_directories(
    include 
    include/core 
    include/cluster 
    include/network 
    external
)

# Source files
set(SOURCES
    src/main.cpp
    src/core/StorageEngine.cpp
    src/core/ConsistentHash.cpp
    src/core/WAL.cpp
    src/cluster/Gossip.cpp
    src/cluster/Replicator.cpp
    src/network/Router.cpp
)

# Executable target
add_executable(kv_server ${SOURCES})

# Link threading library
target_link_libraries(kv_server PRIVATE Threads::Threads)