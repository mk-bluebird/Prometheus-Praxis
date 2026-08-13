// File: cpp/tools/irrigation_mpc_equitable_allocation.cpp
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "../eco_restoration/irrigation_mpc_and_equitable_water.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const IrrigationDynamics dynamics{
            25.0,
            4.0,
            0.05,
            15.0,
            45.0,
            18.0,
            35.0,
            8.0,
            0.08,
            1.20
        };

        const std::vector<RainfallScenario> scenarios{
            {0.35, {0.0, 2.0, 0.0, 1.0}},
            {0.45, {1.0, 0.0, 1.0, 0.0}},
            {0.20, {4.0, 3.0, 2.0, 1.0}}
        };
        const std::vector<std::vector<double>> candidate_schedules{
            {3.0, 4.0, 4.0, 2.0},
            {4.0, 4.0, 3.0, 3.0},
            {2.0, 3.0, 3.0, 2.0},
            {5.0, 5.0, 4.0, 4.0}
        };

        const IrrigationMpcResult irrigation =
            select_robust_irrigation_schedule(
                candidate_schedules, scenarios, dynamics);
        if (!irrigation.robustly_feasible) {
            throw std::runtime_error("no robustly feasible irrigation schedule found");
        }

        const std::vector<WaterStakeholder> stakeholders{
            {10.0, 40.0, [](double water_mm) { return std::sqrt(water_mm); }},
            {10.0, 40.0, [](double water_mm) { return std::sqrt(water_mm); }},
            {10.0, 40.0, [](double water_mm) { return std::sqrt(water_mm); }}
        };
        const std::vector<double> allocation{30.0, 30.0, 30.0};
        const EquitableAllocationResult equity = evaluate_water_allocation(
            allocation, stakeholders, 90.0, 0.01);

        const double irrigation_total = std::accumulate(
            irrigation.schedule_mm.begin(), irrigation.schedule_mm.end(), 0.0);
        const double knowledge_factor = equity.equitable
            ? 0.50 * irrigation.knowledge_factor +
              0.50 * equity.knowledge_factor
            : 0.0;
        const double eco_impact_value = equity.equitable
            ? 0.50 * irrigation.eco_impact_value +
              0.50 * equity.eco_impact_value
            : 0.0;

        std::cout << std::fixed << std::setprecision(6)
                  << "robust_irrigation_feasible="
                  << (irrigation.robustly_feasible ? 1 : 0) << '\n'
                  << "irrigation_total_mm=" << irrigation_total << '\n'
                  << "expected_irrigation_cost=" << irrigation.expected_cost << '\n'
                  << "worst_terminal_moisture_mm="
                  << irrigation.worst_case_terminal_moisture_mm << '\n'
                  << "equitable_allocation=" << (equity.equitable ? 1 : 0) << '\n'
                  << "total_utility=" << equity.total_utility << '\n'
                  << "utility_gap=" << equity.utility_gap << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';

        return equity.equitable ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "irrigation MPC and equitable allocation failed: "
                  << error.what() << '\n';
        return 1;
    }
}
