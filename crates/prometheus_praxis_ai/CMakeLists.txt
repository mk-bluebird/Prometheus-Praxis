cmake_minimum_required(VERSION 3.20)

project(prometheus_praxis_ai
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Root include directory for adapters.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(prometheus_praxis_ai_adapters STATIC
    src/shredding_governance_adapter.cpp
    src/conveyance_conveyor_graph.cpp
    src/wastewater_pump_accountability_adapter.cpp
)

target_include_directories(prometheus_praxis_ai_adapters PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link to ecorestorationshard and other Rust cdylibs at configure/build time.
# Consumers (tests, tools) should add the appropriate -L/path and -lecorestorationshard.
