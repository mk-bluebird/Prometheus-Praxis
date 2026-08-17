#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

struct Assessment {
    double base_radius_m;
    double conservative_radius_m;
    double knowledge_factor;
    double eco_impact_value;
    double harm_risk;
    std::string zone;
    std::string machine_action;
};

static double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

static Assessment assess(
    double breach_flow_lps,
    double surcharge_duration_s,
    double bank_sensitivity,
    double distance_m,
    double energyreqJ,
    double delta_vt
) {
    if (breach_flow_lps <= 0.0 || surcharge_duration_s <= 0.0 || distance_m < 0.0 ||
        energyreqJ < 0.0 || delta_vt < 0.0 || bank_sensitivity < 0.0 || bank_sensitivity > 1.0) {
        throw std::invalid_argument("inputs must be non-negative; flow and duration must be positive; sensitivity must be 0..1");
    }

    const double base_radius_m = std::sqrt(breach_flow_lps * surcharge_duration_s) / 10.0;
    const double conservative_radius_m = base_radius_m * (1.0 + bank_sensitivity * 1.5);
    const double exposure = conservative_radius_m <= 0.0 ? 0.0 : clamp01(1.0 - distance_m / conservative_radius_m);
    const double energy_load = clamp01(energyreqJ / 1000000.0);
    const double velocity_load = clamp01(delta_vt / 10.0);
    const double harm_risk = clamp01(0.60 * exposure + 0.20 * bank_sensitivity + 0.10 * energy_load + 0.10 * velocity_load);
    const double knowledge_factor = clamp01(1.0 - 0.35 * bank_sensitivity - 0.25 * energy_load);
    const double eco_impact_value = clamp01((1.0 - harm_risk) * (0.40 + 0.60 * knowledge_factor));

    std::string zone = "SAFE";
    std::string action = "OPERATE_LOW_IMPACT";
    if (harm_risk >= 0.60) {
        zone = "EXCLUDE";
        action = "NO_ENTRY";
    } else if (harm_risk > 0.25) {
        zone = "CAUTION";
        action = "HOLD_FOR_INSPECTION";
    }

    return {base_radius_m, conservative_radius_m, knowledge_factor, eco_impact_value, harm_risk, zone, action};
}

int main(int argc, char** argv) {
    if (argc != 7) {
        std::cerr << "usage: " << argv[0]
                  << " <breach_flow_lps> <surcharge_duration_s> <bank_sensitivity_0_to_1>"
                  << " <distance_m> <energyreqJ> <delta_vt>\n";
        return 64;
    }

    try {
        const Assessment result = assess(
            std::stod(argv[1]), std::stod(argv[2]), std::stod(argv[3]),
            std::stod(argv[4]), std::stod(argv[5]), std::stod(argv[6])
        );

        std::cout << std::fixed << std::setprecision(3)
                  << "base_radius_m=" << result.base_radius_m << '\n'
                  << "conservative_radius_m=" << result.conservative_radius_m << '\n'
                  << "knowledge_factor=" << result.knowledge_factor << '\n'
                  << "eco_impact_value=" << result.eco_impact_value << '\n'
                  << "harm_risk=" << result.harm_risk << '\n'
                  << "zone=" << result.zone << '\n'
                  << "machine_action=" << result.machine_action << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
