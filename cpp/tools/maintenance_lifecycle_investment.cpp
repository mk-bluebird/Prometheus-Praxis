// File: cpp/tools/maintenance_lifecycle_investment.cpp
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "../eco_restoration/maintenance_and_lifecycle_optimization.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const std::vector<CoolingAsset> assets{
            {0.15, 0.10, 0.22, 1.00, 1.20, 0.55, {120.0, 100.0, 110.0}},
            {0.20, 0.08, 0.18, 1.00, 1.00, 0.58, {90.0, 95.0, 85.0}}
        };
        const MaintenanceScheduleResult maintenance =
            optimize_maintenance_schedule(assets, 3, 300.0);
        if (!maintenance.feasible) {
            throw std::runtime_error("no maintenance schedule satisfies reliability and budget");
        }

        const std::vector<LifecycleAction> investments{
            {"native_shade_canopy", 14.0, 0.92, true, true},
            {"cool_surface_repair", 18.0, 0.78, true, true},
            {"high_emission_cooling", 42.0, 0.83, true, true},
            {"corridor_disruptive_option", 8.0, 0.88, true, false}
        };
        const LifecycleSelection lifecycle =
            select_lifecycle_investments(investments, 0.015);
        if (!lifecycle.has_scalarized_choice) {
            throw std::runtime_error("no safe lifecycle investment satisfies corridor constraints");
        }

        const std::size_t maintenance_count = static_cast<std::size_t>(
            std::count(maintenance.maintenance_actions.begin(),
                       maintenance.maintenance_actions.end(), true));
        const double knowledge_factor =
            0.50 * maintenance.knowledge_factor +
            0.50 * lifecycle.knowledge_factor;
        const double eco_impact_value =
            0.50 * maintenance.eco_impact_value +
            0.50 * lifecycle.eco_impact_value;

        std::cout << std::fixed << std::setprecision(6)
                  << "maintenance_feasible=" << (maintenance.feasible ? 1 : 0) << '\n'
                  << "maintenance_action_count=" << maintenance_count << '\n'
                  << "maintenance_total_cost=" << maintenance.total_cost << '\n'
                  << "minimum_reliability=" << maintenance.minimum_reliability << '\n'
                  << "pareto_action_count=" << lifecycle.pareto_actions.size() << '\n'
                  << "selected_investment="
                  << lifecycle.scalarized_choice.identifier << '\n'
                  << "selected_lifecycle_emissions_kgco2e="
                  << lifecycle.scalarized_choice.lifecycle_emissions_kgco2e << '\n'
                  << "selected_eco_value="
                  << lifecycle.scalarized_choice.eco_value << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "maintenance and lifecycle investment failed: "
                  << error.what() << '\n';
        return 1;
    }
}
