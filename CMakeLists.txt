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

# Collect files
FILE(GLOB SRCFILES src/*.cpp)

# === Include directories ===

# === C++ 17 features ===
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)

# === Linking ===

add_executable(fastsync ${SRCFILES})
#target_link_libraries(schott_plot_2 lmdb nanogui ${OPENGL_LIBRARIES})
