// File: cpp/tools/telemetry_c_abi_layout_test.cpp

#include <cstddef>
#include <cstdint>
#include <iostream>

extern "C" {

struct TelemetryC {
    std::uint64_t hex_anchor;
    std::int64_t observed_unix_s;
    double knowledge_factor;
    double eco_impact_value;
    double risk_of_harm;
    double energy_kwh;
    double carbon_g;
    double heat_risk;
    double water_risk;
    double delta_v;
    std::uint32_t decision_code;
    std::uint32_t sample_count;
    char frame_id[64];
    char owner_did[64];
    char source_id[32];
    std::uint8_t reserved[16];
};

}

static_assert(sizeof(TelemetryC) == 264);
static_assert(alignof(TelemetryC) == 8);
static_assert(offsetof(TelemetryC, hex_anchor) == 0);
static_assert(offsetof(TelemetryC, observed_unix_s) == 8);
static_assert(offsetof(TelemetryC, knowledge_factor) == 16);
static_assert(offsetof(TelemetryC, eco_impact_value) == 24);
static_assert(offsetof(TelemetryC, risk_of_harm) == 32);
static_assert(offsetof(TelemetryC, energy_kwh) == 40);
static_assert(offsetof(TelemetryC, carbon_g) == 48);
static_assert(offsetof(TelemetryC, heat_risk) == 56);
static_assert(offsetof(TelemetryC, water_risk) == 64);
static_assert(offsetof(TelemetryC, delta_v) == 72);
static_assert(offsetof(TelemetryC, decision_code) == 80);
static_assert(offsetof(TelemetryC, sample_count) == 84);
static_assert(offsetof(TelemetryC, frame_id) == 88);
static_assert(offsetof(TelemetryC, owner_did) == 152);
static_assert(offsetof(TelemetryC, source_id) == 216);
static_assert(offsetof(TelemetryC, reserved) == 248);

int main() {
#if defined(__clang__)
    constexpr const char* compiler = "clang";
#elif defined(_MSC_VER)
    constexpr const char* compiler = "msvc";
#elif defined(__GNUC__)
    constexpr const char* compiler = "gcc";
#else
    constexpr const char* compiler = "unknown";
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    constexpr const char* architecture = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    constexpr const char* architecture = "x86_64";
#else
    constexpr const char* architecture = "unknown";
#endif

    std::cout << "{"
              << "\"compiler\":\"" << compiler << "\","
              << "\"architecture\":\"" << architecture << "\","
              << "\"size\":" << sizeof(TelemetryC) << ","
              << "\"alignment\":" << alignof(TelemetryC) << ","
              << "\"offsets\":{"
              << "\"hex_anchor\":" << offsetof(TelemetryC, hex_anchor) << ","
              << "\"observed_unix_s\":" << offsetof(TelemetryC, observed_unix_s) << ","
              << "\"knowledge_factor\":" << offsetof(TelemetryC, knowledge_factor) << ","
              << "\"decision_code\":" << offsetof(TelemetryC, decision_code) << ","
              << "\"frame_id\":" << offsetof(TelemetryC, frame_id) << ","
              << "\"owner_did\":" << offsetof(TelemetryC, owner_did) << ","
              << "\"reserved\":" << offsetof(TelemetryC, reserved)
              << "}}\n";
}
