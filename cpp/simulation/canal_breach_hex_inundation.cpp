// File: cpp/simulation/canal_breach_hex_inundation.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

struct FloodHex {
    std::uint64_t anchor{};
    double area_m2{};
    double conveyance_width_m{};
    double downstream_slope{};
};

struct BreachScenario {
    double inflow_m3_s{};
    double duration_s{};
    double routing_step_s{};
    double manning_n{};
    double depth_reference_m{};
    double confidence{};
};

struct FloodEstimate {
    std::uint64_t anchor{};
    double maximum_depth_m{};
    double r_flood{};
    double knowledge_factor{};
    double eco_impact_value{};
};

std::vector<FloodEstimate> estimate_canal_breach_inundation(
    const std::vector<FloodHex>& traversal, const BreachScenario& scenario) {
    if (traversal.empty() || scenario.inflow_m3_s < 0.0 || scenario.duration_s <= 0.0 ||
        scenario.routing_step_s <= 0.0 || scenario.manning_n <= 0.0 ||
        scenario.depth_reference_m <= 0.0 || scenario.confidence < 0.0 || scenario.confidence > 1.0)
        throw std::invalid_argument("invalid canal breach scenario");

    std::vector<double> volume(traversal.size()), maximum_depth(traversal.size());
    const std::size_t steps = static_cast<std::size_t>(std::ceil(scenario.duration_s / scenario.routing_step_s));
    for (std::size_t step = 0; step < steps; ++step) {
        std::vector<double> transfer(traversal.size());
        volume.front() += scenario.inflow_m3_s * scenario.routing_step_s;
        for (std::size_t i = 0; i < traversal.size(); ++i) {
            const auto& hex = traversal[i];
            if (hex.area_m2 <= 0.0 || hex.conveyance_width_m <= 0.0 || hex.downstream_slope < 0.0)
                throw std::invalid_argument("invalid flood traversal hex");
            const double depth = volume[i] / hex.area_m2;
            maximum_depth[i] = std::max(maximum_depth[i], depth);
            if (i + 1 < traversal.size()) {
                const double hydraulic_radius = std::max(1e-6, depth);
                const double discharge = (1.0 / scenario.manning_n) * hex.conveyance_width_m *
                    hydraulic_radius * std::pow(hydraulic_radius, 2.0 / 3.0) *
                    std::sqrt(hex.downstream_slope);
                transfer[i] = std::min(volume[i], discharge * scenario.routing_step_s);
            }
        }
        for (std::size_t i = 0; i + 1 < traversal.size(); ++i) {
            volume[i] -= transfer[i];
            volume[i + 1] += transfer[i];
        }
    }

    std::vector<FloodEstimate> result;
    for (std::size_t i = 0; i < traversal.size(); ++i) {
        const double risk = std::clamp(maximum_depth[i] / scenario.depth_reference_m, 0.0, 1.0);
        result.push_back({traversal[i].anchor, maximum_depth[i], risk, scenario.confidence,
                          scenario.confidence * (1.0 - risk)});
    }
    return result;
}

}  // namespace eco_restoration
