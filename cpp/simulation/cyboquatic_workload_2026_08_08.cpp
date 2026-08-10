// File: cpp/simulation/cyboquatic_workload_2026_08_08.cpp
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace eco {

struct WorkloadFrame {
    std::string node_id;
    double mass_kg;
    double lift_m;
    double flow_m3_s;
    double head_m;
    double duration_s;
    double renewable_fraction;
    double embodied_carbon_kg_co2e;
};

struct Assessment {
    double energyreq_j;
    double delta_vt;
    double ker_k;
    double ker_e;
    double ker_r;
    double knowledge_factor;
    double eco_impact_value;
    bool accepted;
};

class CyboquaticWorkloadModel {
public:
    Assessment assess(const WorkloadFrame& f) const {
        constexpr double gravity = 9.80665;
        constexpr double water_density = 1000.0;
        const double renewable = std::clamp(f.renewable_fraction, 0.0, 1.0);
        const double lift_energy = std::max(0.0, f.mass_kg) * gravity * std::max(0.0, f.lift_m);
        const double hydraulic_energy =
            water_density * gravity * std::max(0.0, f.flow_m3_s) *
            std::max(0.0, f.head_m) * std::max(0.0, f.duration_s);
        const double energy = lift_energy + hydraulic_energy;
        const double intensity = energy / std::max(1.0, f.duration_s);
        const double carbon_risk = std::clamp(
            (1.0 - renewable) * intensity / 2500.0 + f.embodied_carbon_kg_co2e / 100.0, 0.0, 1.0);
        const double energy_risk = std::clamp(intensity / 3000.0, 0.0, 1.0);
        const double delta_vt = 0.55 * energy_risk * energy_risk + 0.45 * carbon_risk * carbon_risk;
        const double k = 1.0 - delta_vt;
        const double e = renewable * (1.0 - carbon_risk);
        const double r = std::max(energy_risk, carbon_risk);
        const double knowledge = std::clamp(0.55 * k + 0.45 * (1.0 - r), 0.0, 1.0);
        const double impact = std::clamp(0.50 * e + 0.30 * knowledge + 0.20 * (1.0 - delta_vt), 0.0, 1.0);
        return {energy, delta_vt, k, e, r, knowledge, impact, k * e > r && renewable >= 0.70};
    }
};

}  // namespace eco

int main() {
    const eco::WorkloadFrame frame{
        "canal-recovery-pump-08", 120.0, 2.2, 0.018, 1.4, 1800.0, 0.92, 3.4
    };
    const eco::Assessment result = eco::CyboquaticWorkloadModel{}.assess(frame);
    std::cout << std::fixed << std::setprecision(6)
              << "node_id=" << frame.node_id << '\n'
              << "energyreqJ=" << result.energyreq_j << '\n'
              << "delta_vt=" << result.delta_vt << '\n'
              << "K=" << result.ker_k << " E=" << result.ker_e << " R=" << result.ker_r << '\n'
              << "knowledge_factor=" << result.knowledge_factor << '\n'
              << "eco_impact_value=" << result.eco_impact_value << '\n'
              << "accepted=" << (result.accepted ? "true" : "false") << '\n';
}
