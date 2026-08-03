# CMakeLists.txt for cpp/tools
# Minimal wiring for new C++ eco tools as simple executables

cmake_minimum_required(VERSION 3.16)

project(EcoTools
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add minimal C++ eco tools as executables
add_executable(eco_serialization tools/eco_serialization.cpp)
add_executable(eco_profiler tools/eco_profiler.cpp)
add_executable(scenario_template_generator tools/scenario_template_generator.cpp)
add_executable(eco_scenario_library_spec tools/eco_scenario_library_spec.cpp)
add_executable(non_rust_interop_catalog tools/non_rust_interop_catalog.cpp)
add_executable(ai_chat_guidelines_stub tools/ai_chat_guidelines_stub.cpp)
add_executable(collaborator_onboarding_cpp_stub tools/collaborator_onboarding_cpp_stub.cpp)
add_executable(energy_carbon_metrics tools/energy_carbon_metrics.cpp)
add_executable(eco_extension_roadmap tools/eco_extension_roadmap.cpp)

# Optional: Install targets
install(TARGETS
    eco_serialization
    eco_profiler
    scenario_template_generator
    eco_scenario_library_spec
    non_rust_interop_catalog
    ai_chat_guidelines_stub
    collaborator_onboarding_cpp_stub
    energy_carbon_metrics
    eco_extension_roadmap
    RUNTIME DESTINATION bin
)
