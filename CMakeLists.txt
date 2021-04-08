# === Global parameters ===
cmake_minimum_required(VERSION 3.10)
project(fastsync)

# === Options ===

# === System configuration ===

# === Finding packages ===

# === Flags ===
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -std=c++17 -fopenmp")

# === Source files ===
FILE(GLOB SRCFILES src/*.cpp)

# === Include directories ===

# === Targets ===
add_executable(fastsync ${SRCFILES})

# === Linking ===
target_link_libraries(fastsync stdc++fs)
