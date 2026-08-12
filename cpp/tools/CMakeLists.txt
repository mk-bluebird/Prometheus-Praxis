# File: cpp/tools/CMakeLists.txt

add_executable(telemetry_c_abi_layout_test telemetry_c_abi_layout_test.cpp)
target_compile_features(telemetry_c_abi_layout_test PRIVATE cxx_std_20)

add_executable(simd_lane_evaluator ../eco_restoration/simd_lane_evaluator.cpp)
target_compile_features(simd_lane_evaluator PRIVATE cxx_std_20)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME telemetry_c_abi_manifest COMMAND telemetry_c_abi_layout_test)
    add_test(NAME simd_lane_equivalence_and_benchmark COMMAND simd_lane_evaluator)
endif()
