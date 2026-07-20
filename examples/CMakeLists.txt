# filename: examples/CMakeLists.txt
# destination: github.com/mk-bluebird/Prometheus-Praxis/examples/CMakeLists.txt
#
# CMake build file for Prometheus-Praxis C++ examples.
# Provides:
#   - ppx_local_agent        (local agent + telemetry demo)
#   - ppx_observability_demo (diff + telemetry integration demo)

cmake_minimum_required(VERSION 3.16)

project(PrometheusPraxisExamples
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Root of the repo (assuming this CMakeLists is in examples/)
get_filename_component(PPX_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(PPX_CPP_CORE_DIR      "${PPX_ROOT_DIR}/cpp/core")
set(PPX_CPP_TELEMETRY_DIR "${PPX_ROOT_DIR}/cpp/telemetry")
set(PPX_CPP_DIFF_DIR      "${PPX_ROOT_DIR}/cpp/diff")

include_directories(
    "${PPX_CPP_CORE_DIR}"
    "${PPX_CPP_TELEMETRY_DIR}"
    "${PPX_CPP_DIFF_DIR}"
)

# ---------------------------------------------------------------------------
# ppx_local_agent demo
# ---------------------------------------------------------------------------

add_executable(ppx_local_agent
    ppx_local_agent.cpp
)

target_compile_options(ppx_local_agent PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# ---------------------------------------------------------------------------
# ppx_observability_demo
# ---------------------------------------------------------------------------

add_executable(ppx_observability_demo
    ppx_observability_demo.cpp
)

target_compile_options(ppx_observability_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# ---------------------------------------------------------------------------
# Install targets (optional)
# ---------------------------------------------------------------------------

install(TARGETS
    ppx_local_agent
    ppx_observability_demo
    RUNTIME DESTINATION bin
)
