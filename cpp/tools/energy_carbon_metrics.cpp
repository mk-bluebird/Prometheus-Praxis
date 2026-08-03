// File: cpp/tools/energy_carbon_metrics.cpp
#include <iostream>
#include <string>
#include <cmath>

/*
 * Energy-efficiency and carbon-impact metrics for eco workloads.
 *
 * This module defines metrics that map the energy required per eco operation
 * and potential carbon savings into fields suitable for C++ simulations and
 * SQLite schemas. It aligns with the existing ecosafety grammar where
 * workload energy and residual ΔVt are used to gate AI workloads and
 * re-greening actions.[94]
 *
 * Core concepts:
 *   - energy_req_J: Joules consumed by an operation (simulation, AI workload).
 *   - energy_tailwind_J: Joules supplied from low-carbon or surplus sources.
 *   - energy_efficiency_ratio: tailwind / req.
 *   - carbon_intensity_kg_per_kWh: grid or supply-specific factor.
 *   - carbon_emitted_kg: emission for the workload.
 *   - carbon_baseline_kg: emission for a baseline, non-optimised workload.
 *   - carbon_savings_kg: difference between baseline and actual.
 */

struct EnergyCarbonMetrics {
    double energy_req_J;              // Required energy in Joules
    double energy_tailwind_J;         // Low-carbon/surplus energy in Joules
    double carbon_intensity_kg_per_kWh; // Grid carbon intensity
    double carbon_emitted_kg;         // Actual emissions
    double carbon_baseline_kg;        // Baseline emissions
    double carbon_savings_kg;         // Baseline - actual
    double energy_efficiency_ratio;   // energy_tailwind_J / energy_req_J
};

static constexpr double JOULES_PER_KWH = 3.6e6;

EnergyCarbonMetrics compute_energy_carbon(
        double energy_req_J,
        double energy_tailwind_J,
        double carbon_intensity_kg_per_kWh,
        double baseline_intensity_kg_per_kWh)
{
    EnergyCarbonMetrics m{};
    m.energy_req_J = energy_req_J;
    m.energy_tailwind_J = energy_tailwind_J;
    m.carbon_intensity_kg_per_kWh = carbon_intensity_kg_per_kWh;

    // Energy in kWh
    double energy_req_kWh = energy_req_J / JOULES_PER_KWH;

    // Actual emissions: assume fraction of energy_req_J sourced from tailwind pool
    double tailwind_fraction = 0.0;
    if (energy_req_J > 0.0) {
        tailwind_fraction = std::min(1.0, energy_tailwind_J / energy_req_J);
    }
    double grid_fraction = 1.0 - tailwind_fraction;

    double effective_intensity =
        tailwind_fraction * 0.0 /* idealised zero-carbon tailwind */
        + grid_fraction * carbon_intensity_kg_per_kWh;

    m.carbon_emitted_kg = energy_req_kWh * effective_intensity;
    m.carbon_baseline_kg = energy_req_kWh * baseline_intensity_kg_per_kWh;
    m.carbon_savings_kg = m.carbon_baseline_kg - m.carbon_emitted_kg;
    m.energy_efficiency_ratio = (energy_req_J > 0.0)
        ? (energy_tailwind_J / energy_req_J)
        : 0.0;

    return m;
}

/*
 * Example mapping to SQL schema fields:
 *
 *   Table: eco_workload_progress
 *     - workload_id TEXT
 *     - hex_id TEXT
 *     - energyreqJ REAL
 *     - energytailwindJ REAL
 *     - carbon_intensity_kg_per_kWh REAL
 *     - baseline_intensity_kg_per_kWh REAL
 *     - carbon_emitted_kg REAL
 *     - carbon_baseline_kg REAL
 *     - carbon_savings_kg REAL
 *     - energy_efficiency_ratio REAL
 *     - deltaVt REAL  -- Lyapunov residual change for the workload.[94]
 *
 * C++ simulations can populate these fields per workload frame, then KER and
 * governance logic in SQL/ALN can gate workloads based on ΔVt, carbon_savings_kg,
 * and energy_efficiency_ratio.
 */

int main() {
    // Example: 1-hour AI workload consuming 0.5 kWh (~1.8e6 J) with 0.3 kWh tailwind.
    double energy_req_J = 1.8e6;
    double energy_tailwind_J = 1.08e6; // 0.3 kWh
    double carbon_intensity = 0.4;     // kg CO2e per kWh (grid)
    double baseline_intensity = 0.6;   // kg CO2e per kWh (less optimised baseline)

    EnergyCarbonMetrics m = compute_energy_carbon(
        energy_req_J, energy_tailwind_J, carbon_intensity, baseline_intensity);

    std::cout << "Energy-efficiency and carbon-impact metrics:\n";
    std::cout << "  energy_req_J=" << m.energy_req_J << "\n";
    std::cout << "  energy_tailwind_J=" << m.energy_tailwind_J << "\n";
    std::cout << "  energy_efficiency_ratio=" << m.energy_efficiency_ratio << "\n";
    std::cout << "  carbon_emitted_kg=" << m.carbon_emitted_kg << "\n";
    std::cout << "  carbon_baseline_kg=" << m.carbon_baseline_kg << "\n";
    std::cout << "  carbon_savings_kg=" << m.carbon_savings_kg << "\n";

    return 0;
}
