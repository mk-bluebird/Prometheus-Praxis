// File: cpp/eco_restoration/lane_c_abi.cpp

#include <algorithm>
#include <cstddef>
#include <cstdint>

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

int evaluate_telemetry_batch(const TelemetryC* frames, std::size_t count, std::uint8_t* actions) {
    if (frames == nullptr || actions == nullptr) return -1;

    for (std::size_t i = 0; i < count; ++i) {
        const double knowledge = std::clamp(frames[i].knowledge_factor, 0.0, 1.0);
        const double impact = std::clamp(frames[i].eco_impact_value, 0.0, 1.0);
        const double risk = std::clamp(frames[i].risk_of_harm, 0.0, 1.0);
        actions[i] = knowledge < 0.60 || impact < 0.55 || risk > 0.70 ? 2U :
                     risk > 0.35 || impact < 0.63 ? 1U : 0U;
    }
    return 0;
}

}
