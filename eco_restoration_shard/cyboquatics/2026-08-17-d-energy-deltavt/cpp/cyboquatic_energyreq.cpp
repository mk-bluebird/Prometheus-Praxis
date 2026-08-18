#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

static double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

int main(int argc, char** argv) {
    if (argc != 8) {
        std::cerr
            << "usage: " << argv[0]
            << " <force_N> <distance_m> <efficiency_0_to_1> <normal_load_N>"
            << " <contact_area_m2> <allowable_bearing_pressure_Pa> <bank_sensitivity_0_to_1>\n";
        return 64;
    }

    try {
        const double force_n = std::stod(argv[1]);
        const double distance_m = std::stod(argv[2]);
        const double efficiency = std::stod(argv[3]);
        const double normal_load_n = std::stod(argv[4]);
        const double contact_area_m2 = std::stod(argv[5]);
        const double allowable_bearing_pressure_pa = std::stod(argv[6]);
        const double bank_sensitivity = std::stod(argv[7]);

        if (force_n < 0.0 || distance_m < 0.0 || normal_load_n <= 0.0 ||
            contact_area_m2 <= 0.0 || allowable_bearing_pressure_pa <= 0.0 ||
            efficiency <= 0.0 || efficiency > 1.0 ||
            bank_sensitivity < 0.0 || bank_sensitivity > 1.0) {
            throw std::invalid_argument("invalid non-negative values, efficiency, or bank sensitivity");
        }

        const double mechanical_work_j = force_n * distance_m;
        const double energyreq_j = mechanical_work_j / efficiency;
        const double ground_pressure_pa = normal_load_n / contact_area_m2;
        const double allowable_adjusted_pa = allowable_bearing_pressure_pa * (1.0 - bank_sensitivity);
        const bool pressure_safe = ground_pressure_pa <= allowable_adjusted_pa;

        const double pressure_ratio = allowable_adjusted_pa <= 0.0
            ? 1.0
            : ground_pressure_pa / allowable_adjusted_pa;
        const double energy_scale = 1.0e8;
        const double knowledge_factor = clamp01(
            0.85 - 0.30 * bank_sensitivity - 0.15 * std::max(0.0, pressure_ratio - 0.8)
        );
        const double harm_risk = clamp01(
            0.10 + 0.55 * bank_sensitivity + 0.35 * std::max(0.0, pressure_ratio - 0.8)
        );
        const double eco_impact_value = clamp01(
            knowledge_factor * (1.0 - harm_risk) * (1.0 - clamp01(energyreq_j / energy_scale) * 0.20)
        );

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "mechanical_work_J=" << mechanical_work_j << '\n';
        std::cout << "energyreqJ=" << energyreq_j << '\n';
        std::cout << "ground_pressure_Pa=" << ground_pressure_pa << '\n';
        std::cout << "adjusted_allowable_pressure_Pa=" << allowable_adjusted_pa << '\n';
        std::cout << "pressure_ratio=" << pressure_ratio << '\n';
        std::cout << "operation_screen=" << (pressure_safe ? "ELIGIBLE_FOR_SITE_REVIEW" : "REJECT_GROUND_PRESSURE") << '\n';
        std::cout << "knowledge_factor=" << knowledge_factor << '\n';
        std::cout << "eco_impact_value=" << eco_impact_value << '\n';
        std::cout << "harm_risk=" << harm_risk << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
