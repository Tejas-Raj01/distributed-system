cmake_minimum_required(VERSION 3.10)
project(KVStoreServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)

# Compiler ko batayein ki saari Header (.hpp) files kahan rakhi hain
# Taaki aapko .cpp files mein "#include <core/StorageEngine.hpp>" na likhna pade
include_directories(
    include 
    include/core 
    include/cluster 
    include/network 
    external
)

# Saari Source (.cpp) files ka sahi rasta (path)
set(SOURCES
    src/main.cpp
    src/core/StorageEngine.cpp
    src/core/ConsistentHash.cpp
    src/core/WAL.cpp
    src/cluster/Gossip.cpp
    src/cluster/Replicator.cpp
    src/network/Router.cpp
)

# Executable (Program) banayein
add_executable(kv_server ${SOURCES})

# Threads library link karein
target_link_libraries(kv_server PRIVATE Threads::Threads)