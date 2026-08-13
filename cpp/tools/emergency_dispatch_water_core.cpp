// File: cpp/tools/emergency_dispatch_water_core.cpp
#include <array>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../eco_restoration/emergency_dispatch_and_water_core.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const DispatchConstraints constraints{
            15.0,
            50.0,
            0.30,
            0.02,
            0.03,
            0.10
        };
        const std::vector<EmergencyScenario> scenarios{
            {{20.0, 19.0, 18.0}, {53.0, 52.0, 51.0}, {0.12, 0.12, 0.13},
             10.0, 6.0, 0.04, 0.03},
            {{22.0, 21.0, 19.0}, {54.0, 53.0, 52.0}, {0.14, 0.14, 0.15},
             12.0, 7.0, 0.05, 0.03}
        };
        const std::vector<std::vector<DispatchAction>> plans{
            {{0.80, 0.75}, {0.80, 0.75}, {0.75, 0.80}},
            {{0.60, 0.60}, {0.60, 0.60}, {0.60, 0.60}},
            {{0.90, 0.90}, {0.85, 0.85}, {0.80, 0.80}}
        };

        const RobustDispatchResult dispatch = select_robust_dispatch(
            plans, scenarios, constraints, {0.50, 0.50});
        if (!dispatch.feasible) {
            throw std::runtime_error("no robust dispatch plan satisfies heat, delay, and RoH bounds");
        }

        ThreeUserWaterGame game;
        game.coalition_values = {
            0.0,
            20.0,
            25.0,
            50.0,
            20.0,
            48.0,
            52.0,
            75.0
        };
        const CoreAllocation allocation =
            find_three_user_core_allocation(game, 1.0);
        if (!allocation.stable) {
            throw std::runtime_error("no grid-resolved core allocation found");
        }

        const double knowledge_factor =
            0.50 * dispatch.knowledge_factor +
            0.50 * allocation.knowledge_factor;
        const double eco_impact_value =
            0.50 * dispatch.eco_impact_value +
            0.50 * allocation.eco_impact_value;

        std::cout << std::fixed << std::setprecision(6)
                  << "robust_dispatch_feasible=" << (dispatch.feasible ? 1 : 0) << '\n'
                  << "robust_dispatch_cost=" << dispatch.robust_cost << '\n'
                  << "maximum_delay_s=" << dispatch.maximum_delay_s << '\n'
                  << "maximum_heat_index=" << dispatch.maximum_heat_index << '\n'
                  << "maximum_risk_of_harm="
                  << dispatch.maximum_risk_of_harm << '\n'
                  << "agricultural_core_payoff=" << allocation.payoffs[0] << '\n'
                  << "municipal_core_payoff=" << allocation.payoffs[1] << '\n'
                  << "ecological_core_payoff=" << allocation.payoffs[2] << '\n'
                  << "core_stable=" << (allocation.stable ? 1 : 0) << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "emergency dispatch and water-core assessment failed: "
                  << error.what() << '\n';
        return 1;
    }
}
