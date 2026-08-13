// File: cpp/eco_restoration/irrigation_mpc_and_equitable_water.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

/*
For rainfall scenario xi, root-zone moisture evolves as:
theta_(t+1)=theta_t+u_t+rain_t(xi)-ET_t-deep_drainage(theta_t).

Choose a receding-horizon schedule u_0,...,u_(T-1) minimizing expected:
sum_t c_u*u_t^2+c_s*max(0,theta_min-theta_t)^2.

Robust horizon constraints must hold for every retained rainfall scenario:
0<=u_t<=u_max,
theta_min<=theta_t<=theta_max,
theta_T in [theta_terminal_min,theta_terminal_max].

The terminal interval is a robust control-invariant moisture set. It prevents a
short-horizon optimizer from conserving water by leaving roots too dry at the
end of its planning horizon.
*/
struct RainfallScenario {
    double probability{};
    std::vector<double> rainfall_mm;
};

struct IrrigationDynamics {
    double initial_moisture_mm{};
    double evapotranspiration_mm_per_step{};
    double drainage_fraction{};
    double moisture_min_mm{};
    double moisture_max_mm{};
    double terminal_min_mm{};
    double terminal_max_mm{};
    double irrigation_max_mm_per_step{};
    double irrigation_cost{};
    double stress_cost{};
};

struct IrrigationMpcResult {
    std::vector<double> schedule_mm;
    double expected_cost{};
    double worst_case_terminal_moisture_mm{};
    bool robustly_feasible{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline double next_soil_moisture(double moisture_mm, double irrigation_mm,
                                 double rainfall_mm,
                                 const IrrigationDynamics& dynamics) {
    const double excess = std::max(0.0, moisture_mm - dynamics.moisture_max_mm);
    return moisture_mm + irrigation_mm + rainfall_mm -
           dynamics.evapotranspiration_mm_per_step -
           dynamics.drainage_fraction * excess;
}

inline IrrigationMpcResult select_robust_irrigation_schedule(
    const std::vector<std::vector<double>>& candidate_schedules,
    const std::vector<RainfallScenario>& scenarios,
    const IrrigationDynamics& dynamics) {

    if (candidate_schedules.empty() || scenarios.empty() ||
        dynamics.moisture_min_mm > dynamics.moisture_max_mm ||
        dynamics.terminal_min_mm > dynamics.terminal_max_mm ||
        dynamics.irrigation_max_mm_per_step < 0.0) {
        throw std::invalid_argument("invalid irrigation MPC inputs");
    }

    IrrigationMpcResult best;
    best.expected_cost = std::numeric_limits<double>::infinity();

    for (const auto& schedule : candidate_schedules) {
        if (schedule.empty()) continue;
        bool feasible = true;
        double expected_cost = 0.0;
        double worst_terminal = std::numeric_limits<double>::infinity();

        for (const auto& scenario : scenarios) {
            if (scenario.rainfall_mm.size() != schedule.size() ||
                scenario.probability < 0.0) {
                throw std::invalid_argument("rainfall scenario dimensions differ");
            }

            double moisture = dynamics.initial_moisture_mm;
            double scenario_cost = 0.0;
            for (std::size_t step = 0; step < schedule.size(); ++step) {
                const double irrigation = schedule[step];
                if (irrigation < 0.0 ||
                    irrigation > dynamics.irrigation_max_mm_per_step) {
                    feasible = false;
                    break;
                }
                scenario_cost += dynamics.irrigation_cost * irrigation * irrigation;
                scenario_cost += dynamics.stress_cost *
                    std::pow(std::max(0.0, dynamics.moisture_min_mm - moisture), 2.0);
                moisture = next_soil_moisture(
                    moisture, irrigation, scenario.rainfall_mm[step], dynamics);
                if (moisture < dynamics.moisture_min_mm ||
                    moisture > dynamics.moisture_max_mm) {
                    feasible = false;
                    break;
                }
            }

            if (!feasible || moisture < dynamics.terminal_min_mm ||
                moisture > dynamics.terminal_max_mm) {
                feasible = false;
                break;
            }
            expected_cost += scenario.probability * scenario_cost;
            worst_terminal = std::min(worst_terminal, moisture);
        }

        if (feasible && expected_cost < best.expected_cost) {
            const double water_total = std::accumulate(schedule.begin(), schedule.end(), 0.0);
            const double possible_total = dynamics.irrigation_max_mm_per_step *
                static_cast<double>(schedule.size());
            const double conservation = 1.0 - water_total / std::max(possible_total, 1e-9);
            best = {schedule, expected_cost, worst_terminal, true,
                    std::clamp(0.80 + 0.20 * conservation, 0.0, 1.0),
                    std::clamp(0.55 + 0.45 * conservation, 0.0, 1.0)};
        }
    }
    return best;
}

/*
For stakeholder allocations w_i, maximize sum_i U_i(w_i), subject to:
sum_i w_i<=available_water,
w_i>=minimum_allocation_i,
max_(i,j)|U_i(w_i)-U_j(w_j)|<=equity_tolerance.

The fair-efficiency tradeoff is the utility gap:
Delta(epsilon)=U_unconstrained-U_equity(epsilon).
As epsilon decreases, the feasible set contracts. Report Delta along with
minimum-service compliance; do not claim an allocation is equitable merely
because its aggregate utility is high.
*/
struct WaterStakeholder {
    double minimum_allocation_mm{};
    double maximum_allocation_mm{};
    std::function<double(double)> utility;
};

struct EquitableAllocationResult {
    std::vector<double> allocation_mm;
    double total_utility{};
    double utility_gap{};
    bool equitable{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline EquitableAllocationResult evaluate_water_allocation(
    const std::vector<double>& allocation_mm,
    const std::vector<WaterStakeholder>& stakeholders,
    double available_water_mm, double equity_tolerance) {

    if (allocation_mm.size() != stakeholders.size() || stakeholders.empty() ||
        available_water_mm < 0.0 || equity_tolerance < 0.0) {
        throw std::invalid_argument("invalid water-allocation inputs");
    }

    double total_water = 0.0;
    double total_utility = 0.0;
    double minimum_utility = std::numeric_limits<double>::infinity();
    double maximum_utility = -std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < stakeholders.size(); ++i) {
        const auto& stakeholder = stakeholders[i];
        if (!stakeholder.utility || allocation_mm[i] < stakeholder.minimum_allocation_mm ||
            allocation_mm[i] > stakeholder.maximum_allocation_mm) {
            throw std::invalid_argument("allocation violates stakeholder bounds");
        }
        const double utility = stakeholder.utility(allocation_mm[i]);
        total_water += allocation_mm[i];
        total_utility += utility;
        minimum_utility = std::min(minimum_utility, utility);
        maximum_utility = std::max(maximum_utility, utility);
    }

    const double gap = maximum_utility - minimum_utility;
    const bool equitable = total_water <= available_water_mm && gap <= equity_tolerance;
    const double knowledge = std::clamp(
        1.0 - gap / std::max(1.0, equity_tolerance + 1.0), 0.0, 1.0);
    const double impact = equitable
        ? std::clamp(total_utility / (std::abs(total_utility) + 1.0), 0.0, 1.0)
        : 0.0;
    return {allocation_mm, total_utility, gap, equitable, knowledge, impact};
}

}  // namespace eco_restoration
