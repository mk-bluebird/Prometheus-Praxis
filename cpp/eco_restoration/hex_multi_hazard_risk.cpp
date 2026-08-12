// File: cpp/eco_restoration/hex_multi_hazard_risk.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace eco_restoration {

struct HexHazardInputs {
    std::uint64_t anchor{};
    double heat_risk{};
    double canal_surcharge_depth_m{};
    double canal_surcharge_distance_m{};
    double drought_deficit_mm{};
    double flood_depth_reference_m{};
    double flood_decay_distance_m{};
    double drought_reference_mm{};
    double confidence{};
};

struct MultiHazardRisk {
    double heat{};
    double flood{};
    double drought{};
    double composite{};
    double knowledge_factor{};
    double eco_impact_value{};
};

MultiHazardRisk evaluate_multi_hazard(const HexHazardInputs& in) {
    if (in.canal_surcharge_depth_m < 0.0 || in.canal_surcharge_distance_m < 0.0 ||
        in.drought_deficit_mm < 0.0 || in.flood_depth_reference_m <= 0.0 ||
        in.flood_decay_distance_m <= 0.0 || in.drought_reference_mm <= 0.0 ||
        in.confidence < 0.0 || in.confidence > 1.0)
        throw std::invalid_argument("invalid hex hazard inputs");

    const double heat = std::clamp(in.heat_risk, 0.0, 1.0);
    const double flood = std::clamp(in.canal_surcharge_depth_m / in.flood_depth_reference_m *
        std::exp(-in.canal_surcharge_distance_m / in.flood_decay_distance_m), 0.0, 1.0);
    const double drought = std::clamp(in.drought_deficit_mm / in.drought_reference_mm, 0.0, 1.0);
    const double composite = 1.0 - (1.0 - heat) * (1.0 - flood) * (1.0 - drought);
    const double knowledge = std::clamp(in.confidence, 0.0, 1.0);
    return {heat, flood, drought, composite, knowledge, knowledge * (1.0 - composite)};
}

}  // namespace eco_restoration
