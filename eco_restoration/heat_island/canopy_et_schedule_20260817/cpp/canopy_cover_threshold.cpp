#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

struct CanopyResult {
    double unconstrained_fraction;
    double required_fraction;
    std::string status;
    double knowledge_factor;
    double eco_impact_value;
    double harm_risk;
};

static double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

static CanopyResult calculate(
    double pavement_temperature_c,
    double vegetation_temperature_c,
    double target_temperature_c,
    double existing_canopy_fraction
) {
    if (!std::isfinite(pavement_temperature_c) ||
        !std::isfinite(vegetation_temperature_c) ||
        !std::isfinite(target_temperature_c) ||
        !std::isfinite(existing_canopy_fraction) ||
        existing_canopy_fraction < 0.0 ||
        existing_canopy_fraction > 1.0) {
        throw std::invalid_argument("inputs must be finite; canopy fraction must be in [0, 1]");
    }

    if (target_temperature_c >= pavement_temperature_c) {
        return {0.0, 0.0, "ALREADY_MET", 0.85, 0.50, 0.10};
    }

    if (vegetation_temperature_c >= pavement_temperature_c) {
        return {1.0, 1.0, "NO_COOLING_CONTRAST", 0.30, 0.05, 0.70};
    }

    const double unconstrained = (
        pavement_temperature_c - target_temperature_c
    ) / (
        pavement_temperature_c - vegetation_temperature_c
    );

    if (unconstrained > 1.0) {
        return {
            unconstrained,
            1.0,
            "IMPOSSIBLE_WITH_DECLARED_VEGETATED_SURFACE_TEMPERATURE",
            0.45,
            0.10,
            0.75
        };
    }

    const double required = clamp01(unconstrained);
    const double deficit = std::max(0.0, required - existing_canopy_fraction);
    const double knowledge = clamp01(0.70 + 0.15 * (vegetation_temperature_c < pavement_temperature_c ? 1.0 : 0.0));
    const double impact = clamp01((1.0 - required) * 0.20 + required * 0.85);
    const double risk = clamp01(0.15 + 0.55 * deficit);

    return {
        unconstrained,
        required,
        deficit <= 1.0e-12 ? "TARGET_MET_BY_EXISTING_CANOPY" : "CANOPY_EXPANSION_REQUIRED",
        knowledge,
        impact,
        risk
    };
}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr
            << "usage: " << argv[0]
            << " <T_pavement_C> <T_vegetation_C> <T_target_C> <existing_canopy_fraction_0_to_1>\n";
        return 64;
    }

    try {
        const CanopyResult result = calculate(
            std::stod(argv[1]),
            std::stod(argv[2]),
            std::stod(argv[3]),
            std::stod(argv[4])
        );

        std::cout << std::fixed << std::setprecision(8);
        std::cout << "unconstrained_canopy_fraction=" << result.unconstrained_fraction << '\n';
        std::cout << "required_canopy_fraction=" << result.required_fraction << '\n';
        std::cout << "status=" << result.status << '\n';
        std::cout << "knowledge_factor=" << result.knowledge_factor << '\n';
        std::cout << "eco_impact_value=" << result.eco_impact_value << '\n';
        std::cout << "harm_risk=" << result.harm_risk << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
