# Top-level CMakeLists.txt for Prometheus-Praxis
# Provides build configuration for C++ eco tools

cmake_minimum_required(VERSION 3.16)

project(PrometheusPraxis
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add subdirectories for C++ components (tools only)
add_subdirectory(cpp)
