# CMakeLists.txt for cpp directory
# Minimal wiring for new C++ eco tools as simple executables

cmake_minimum_required(VERSION 3.16)

project(EcoTools
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Eco-restoration executables from cpp/eco_restoration
add_executable(eco_workload_admission eco_restoration/eco_workload_admission.cpp)
add_executable(research_corridor_registry eco_restoration/research_corridor_registry.cpp)

# Simulation executables from cpp/simulation
add_executable(eco_workload_planner simulation/cyboquatic_workload_model.cpp)
add_executable(hex_anchor_geodesic_calibration simulation/hex_anchor_geodesic_calibration.cpp)
add_executable(canal_lyapunov_calibration simulation/canal_lyapunov_calibration.cpp)
add_executable(delayed_cbf_ker_sensitivity simulation/delayed_cbf_ker_sensitivity.cpp)
add_executable(nsga_lane_threshold_tuning simulation/nsga_lane_threshold_tuning.cpp)
add_executable(battery_chance_constrained_scheduler simulation/battery_chance_constrained_scheduler.cpp)
add_executable(carbon_and_water_risk_models simulation/carbon_and_water_risk_models.cpp)

# Link Eigen3::Eigen to specified targets
if(TARGET Eigen3::Eigen)
    target_link_libraries(hex_anchor_geodesic_calibration PRIVATE Eigen3::Eigen)
    target_link_libraries(canal_lyapunov_calibration PRIVATE Eigen3::Eigen)
    target_link_libraries(nsga_lane_threshold_tuning PRIVATE Eigen3::Eigen)
    target_link_libraries(carbon_and_water_risk_models PRIVATE Eigen3::Eigen)
endif()

# Link SQLite::SQLite3 to specified targets
if(TARGET SQLite::SQLite3)
    target_link_libraries(hex_anchor_geodesic_calibration PRIVATE SQLite::SQLite3)
    target_link_libraries(delayed_cbf_ker_sensitivity PRIVATE SQLite::SQLite3)
    target_link_libraries(nsga_lane_threshold_tuning PRIVATE SQLite::SQLite3)
    target_link_libraries(battery_chance_constrained_scheduler PRIVATE SQLite::SQLite3)
    target_link_libraries(carbon_and_water_risk_models PRIVATE SQLite::SQLite3)
endif()

# Link PROJ::proj to specified targets
if(TARGET PROJ::proj)
    target_link_libraries(hex_anchor_geodesic_calibration PRIVATE PROJ::proj)
endif()

# Add minimal C++ eco tools as executables (existing)
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
    eco_workload_admission
    research_corridor_registry
    eco_workload_planner
    hex_anchor_geodesic_calibration
    canal_lyapunov_calibration
    delayed_cbf_ker_sensitivity
    nsga_lane_threshold_tuning
    battery_chance_constrained_scheduler
    carbon_and_water_risk_models
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

# Enable testing if requested
enable_testing()
