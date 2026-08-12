// File: cpp/eco_restoration/hardware_lca_energy_risk.cpp

#include <algorithm>
#include <stdexcept>

namespace eco_restoration {

struct HardwareLca {
    double embodied_carbon_g{};
    double lifespan_hours{};
    double power_rating_kw{};
    double allocation_beta{1.0};
};

struct EnergyRiskResult {
    double operational_carbon_g{};
    double embodied_carbon_g{};
    double total_carbon_g{};
    double operational_risk{};
    double total_risk{};
};

EnergyRiskResult energy_risk_with_lca(
    double workload_energy_kwh,
    double renewable_fraction,
    double grid_carbon_g_per_kwh,
    double approved_carbon_budget_g,
    const HardwareLca& hardware) {

    if (workload_energy_kwh < 0.0 || renewable_fraction < 0.0 || renewable_fraction > 1.0 ||
        grid_carbon_g_per_kwh < 0.0 || approved_carbon_budget_g <= 0.0 ||
        hardware.embodied_carbon_g < 0.0 || hardware.lifespan_hours <= 0.0 ||
        hardware.power_rating_kw <= 0.0 || hardware.allocation_beta < 0.0 ||
        hardware.allocation_beta > 1.0) {
        throw std::invalid_argument("invalid LCA energy-risk input");
    }

    const double operational_carbon =
        workload_energy_kwh * (1.0 - renewable_fraction) * grid_carbon_g_per_kwh;

    const double embodied_g_per_kwh =
        hardware.allocation_beta * hardware.embodied_carbon_g /
        (hardware.lifespan_hours * hardware.power_rating_kw);

    const double embodied_carbon = workload_energy_kwh * embodied_g_per_kwh;
    const double total_carbon = operational_carbon + embodied_carbon;

    return {
        operational_carbon,
        embodied_carbon,
        total_carbon,
        std::clamp(operational_carbon / approved_carbon_budget_g, 0.0, 1.0),
        std::clamp(total_carbon / approved_carbon_budget_g, 0.0, 1.0)
    };
}

}  // namespace eco_restoration
