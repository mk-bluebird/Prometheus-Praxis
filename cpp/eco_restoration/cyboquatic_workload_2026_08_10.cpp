// File: cpp/eco_restoration/cyboquatic_workload_2026_08_10.cpp
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct TelemetryFrame {
    std::string node_id;
    double flow_m3_s;
    double hydraulic_head_m;
    double pump_efficiency;
    double renewable_fraction;
    double biodiversity_risk;
    double sediment_risk;
};

struct WorkloadAssessment {
    double energyreq_j;
    double delta_vt;
    double knowledge_factor;
    double eco_impact_value;
    bool acceptable;
};

class CyboquaticWorkloadModel {
public:
    WorkloadAssessment assess(const TelemetryFrame& frame) const {
        validate(frame);

        constexpr double water_density_kg_m3 = 998.0;
        constexpr double gravity_m_s2 = 9.80665;
        constexpr double sample_seconds = 60.0;

        const double hydraulic_energy =
            water_density_kg_m3 * gravity_m_s2 * frame.flow_m3_s *
            frame.hydraulic_head_m * sample_seconds;
        const double energyreq_j = hydraulic_energy / frame.pump_efficiency;

        const double energy_risk =
            std::clamp((1.0 - frame.renewable_fraction) * energyreq_j / 2.0e6, 0.0, 1.0);
        const double delta_vt =
            energy_risk * energy_risk +
            frame.biodiversity_risk * frame.biodiversity_risk +
            frame.sediment_risk * frame.sediment_risk;

        const double evidence_quality =
            1.0 - std::abs(frame.flow_m3_s - std::round(frame.flow_m3_s * 100.0) / 100.0);
        const double knowledge_factor = std::clamp(
            0.45 * frame.pump_efficiency +
            0.35 * frame.renewable_fraction +
            0.20 * evidence_quality, 0.0, 1.0);

        const double eco_impact_value = std::clamp(
            knowledge_factor *
            (1.0 - 0.45 * energy_risk -
             0.30 * frame.biodiversity_risk -
             0.25 * frame.sediment_risk),
            0.0, 1.0);

        return {energyreq_j, delta_vt, knowledge_factor, eco_impact_value,
                frame.biodiversity_risk <= 0.25 &&
                frame.sediment_risk <= 0.25 &&
                frame.renewable_fraction >= 0.60 &&
                eco_impact_value >= 0.55};
    }

private:
    static void validate(const TelemetryFrame& frame) {
        if (frame.node_id.empty() || frame.flow_m3_s < 0.0 ||
            frame.hydraulic_head_m < 0.0 || frame.pump_efficiency <= 0.0 ||
            frame.pump_efficiency > 1.0 || frame.renewable_fraction < 0.0 ||
            frame.renewable_fraction > 1.0 || frame.biodiversity_risk < 0.0 ||
            frame.biodiversity_risk > 1.0 || frame.sediment_risk < 0.0 ||
            frame.sediment_risk > 1.0) {
            throw std::invalid_argument("Telemetry values violate ecological operating bounds.");
        }
    }
};

}  // namespace eco_restoration

int main(int argc, char* argv[]) {
    if (argc != 7) {
        std::cerr << "Usage: cyboquatic_workload NODE FLOW_M3_S HEAD_M EFFICIENCY RENEWABLE_FRACTION BIODIVERSITY_RISK\n";
        return EXIT_FAILURE;
    }

    try {
        const double sediment_risk = 0.10;
        const eco_restoration::TelemetryFrame frame{
            argv[1], std::stod(argv[2]), std::stod(argv[3]), std::stod(argv[4]),
            std::stod(argv[5]), std::stod(argv[6]), sediment_risk};

        const auto result = eco_restoration::CyboquaticWorkloadModel{}.assess(frame);
        std::cout << std::fixed << std::setprecision(6)
                  << "node_id=" << frame.node_id
                  << ",energyreqJ=" << result.energyreq_j
                  << ",deltaVt=" << result.delta_vt
                  << ",knowledge_factor=" << result.knowledge_factor
                  << ",eco_impact_value=" << result.eco_impact_value
                  << ",acceptable=" << (result.acceptable ? 1 : 0) << '\n';
        return result.acceptable ? EXIT_SUCCESS : 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
