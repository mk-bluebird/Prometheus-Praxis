// File: cpp/simulation/cyboquatic_workload_2026_08_09.cpp
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace cyboquatic {

struct Telemetry {
    std::string node_id;
    double flow_m3_s;
    double lift_m;
    double efficiency;
    double runtime_s;
    double voltage_drop_v;
    double renewable_fraction;
    double embodied_carbon_g_per_j;
    double biodiversity_risk;
};

struct Assessment {
    double energyreq_j;
    double delta_vt;
    double knowledge_factor;
    double eco_impact_value;
    bool accepted;
};

Assessment assess(const Telemetry& t) {
    const bool valid =
        !t.node_id.empty() && t.flow_m3_s >= 0.0 && t.lift_m >= 0.0 &&
        t.efficiency > 0.0 && t.efficiency <= 1.0 && t.runtime_s >= 0.0 &&
        t.voltage_drop_v >= 0.0 && t.renewable_fraction >= 0.0 &&
        t.renewable_fraction <= 1.0 && t.embodied_carbon_g_per_j >= 0.0 &&
        t.biodiversity_risk >= 0.0 && t.biodiversity_risk <= 1.0;

    if (!valid) {
        return {0.0, 0.0, 0.0, 0.0, false};
    }

    constexpr double water_density_kg_m3 = 997.0;
    constexpr double gravity_m_s2 = 9.80665;
    const double hydraulic_j =
        water_density_kg_m3 * gravity_m_s2 * t.flow_m3_s * t.lift_m * t.runtime_s;
    const double energyreq_j = hydraulic_j / t.efficiency;
    const double renewable_energy_j = energyreq_j * t.renewable_fraction;
    const double grid_energy_j = energyreq_j - renewable_energy_j;
    const double carbon_g = grid_energy_j * t.embodied_carbon_g_per_j;
    const double delta_vt =
        0.55 * std::min(1.0, carbon_g / 1000.0) +
        0.30 * std::min(1.0, t.voltage_drop_v / 24.0) +
        0.15 * t.biodiversity_risk;

    const double measurement_completeness =
        (t.flow_m3_s > 0.0 ? 0.25 : 0.0) +
        (t.lift_m >= 0.0 ? 0.25 : 0.0) +
        (t.runtime_s > 0.0 ? 0.25 : 0.0) +
        (t.efficiency > 0.0 ? 0.25 : 0.0);
    const double knowledge_factor =
        std::clamp(measurement_completeness * (1.0 - 0.5 * t.biodiversity_risk), 0.0, 1.0);
    const double eco_impact_value =
        std::clamp((0.55 * t.renewable_fraction + 0.45 * (1.0 - delta_vt)) *
                   (1.0 - t.biodiversity_risk), 0.0, 1.0);
    const bool accepted =
        delta_vt <= 0.35 && eco_impact_value >= 0.60 && knowledge_factor >= 0.75;
    return {energyreq_j, delta_vt, knowledge_factor, eco_impact_value, accepted};
}

}  // namespace cyboquatic

int main() {
    const cyboquatic::Telemetry sample{
        "phoenix-canal-pump-01", 0.035, 4.2, 0.78, 900.0, 2.1, 0.82, 0.000035, 0.08};
    const auto result = cyboquatic::assess(sample);

    std::cout << std::fixed << std::setprecision(6)
              << "node_id=" << sample.node_id << '\n'
              << "energyreqJ=" << result.energyreq_j << '\n'
              << "deltaVt=" << result.delta_vt << '\n'
              << "knowledge_factor=" << result.knowledge_factor << '\n'
              << "eco_impact_value=" << result.eco_impact_value << '\n'
              << "accepted=" << (result.accepted ? "1" : "0") << '\n';
    return result.accepted ? 0 : 2;
}
