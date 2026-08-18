#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

struct HeatSinkResult {
    double available_heat_j;
    double accepted_heat_j;
    double unmet_demand_j;
    std::string status;
    double knowledge_factor;
    double eco_impact_value;
    double harm_risk;
};

static double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

static HeatSinkResult assess_heat_sink(
    const std::string& sink_type,
    double ai_energy_j,
    double heat_efficiency,
    double sink_heat_demand_j,
    double current_temperature_c,
    double target_max_temperature_c,
    bool water_body_connected
) {
    if (ai_energy_j < 0.0 || heat_efficiency < 0.0 || heat_efficiency > 1.0 ||
        sink_heat_demand_j < 0.0 || !std::isfinite(current_temperature_c) ||
        !std::isfinite(target_max_temperature_c)) {
        throw std::invalid_argument("invalid heat telemetry input");
    }

    const double available_heat_j = heat_efficiency * ai_energy_j;
    const bool canal_rejected = sink_type == "CANAL_WATER" && water_body_connected;
    const bool temperature_safe = current_temperature_c <= target_max_temperature_c;

    if (canal_rejected) {
        return {
            available_heat_j, 0.0, sink_heat_demand_j,
            "REJECT_CANAL_WATER_WARMING",
            0.40, 0.10, 0.90
        };
    }

    if (!temperature_safe) {
        return {
            available_heat_j, 0.0, sink_heat_demand_j,
            "HOLD_OVER_TARGET_TEMPERATURE",
            0.60, 0.20, 0.70
        };
    }

    const double accepted_heat_j = std::min(available_heat_j, sink_heat_demand_j);
    const double unmet_demand_j = std::max(0.0, sink_heat_demand_j - accepted_heat_j);
    const double demand_fraction = sink_heat_demand_j <= 0.0 ? 0.0 : accepted_heat_j / sink_heat_demand_j;

    return {
        available_heat_j,
        accepted_heat_j,
        unmet_demand_j,
        "ELIGIBLE_FOR_SITE_THERMAL_REVIEW",
        0.80,
        clamp01(0.45 + 0.45 * demand_fraction),
        0.20
    };
}

int main(int argc, char** argv) {
    if (argc != 7) {
        std::cerr
            << "usage: " << argv[0]
            << " <GREENHOUSE|COMPOST|CANAL_WATER> <E_AI_J> <eta_heat_0_to_1>"
            << " <heat_demand_J> <current_temp_C> <target_max_temp_C>\n";
        return 64;
    }

    try {
        const HeatSinkResult result = assess_heat_sink(
            argv[1],
            std::stod(argv[2]),
            std::stod(argv[3]),
            std::stod(argv[4]),
            std::stod(argv[5]),
            std::stod(argv[6]),
            std::string(argv[1]) == "CANAL_WATER"
        );

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "Q_reuse_J=" << result.available_heat_j << '\n';
        std::cout << "accepted_heat_J=" << result.accepted_heat_j << '\n';
        std::cout << "unmet_heat_demand_J=" << result.unmet_demand_j << '\n';
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
