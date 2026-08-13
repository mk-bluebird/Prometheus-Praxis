// File: cpp/eco_restoration/emergency_dispatch_and_water_core.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

/*
Robust receding-horizon policy:
at each update, solve over candidate u_(0:H-1) and every retained disturbance
scenario xi:
min sum_t q_h*e_h(t)^2+q_v*e_v(t)^2+q_delta*||u_t-u_(t-1)||^2
s.t. delay_t(xi)<=d_max, heat_j,t(xi)<H_crit for every zone j,
     RoH_t(xi)<=0.30, and terminal recovery constraints.

Apply only u_0, observe updated delay/heat/risk, then solve again. The planner
returns advisory dispatch weights only; it does not interface with traffic or
cooling equipment.
*/
struct DispatchAction {
    double traffic_priority{};
    double cooling_priority{};
};

struct EmergencyScenario {
    std::vector<double> baseline_delay_s;
    std::vector<double> baseline_heat_index;
    std::vector<double> baseline_risk_of_harm;
    double traffic_delay_reduction_s{};
    double cooling_heat_reduction{};
    double traffic_priority_risk{};
    double cooling_priority_risk{};
};

struct DispatchConstraints {
    double maximum_delay_s{};
    double heat_critical{};
    double risk_of_harm_limit{0.30};
    double traffic_weight{};
    double cooling_weight{};
    double change_weight{};
};

struct RobustDispatchResult {
    std::vector<DispatchAction> plan;
    double robust_cost{};
    double maximum_delay_s{};
    double maximum_heat_index{};
    double maximum_risk_of_harm{};
    bool feasible{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline RobustDispatchResult select_robust_dispatch(
    const std::vector<std::vector<DispatchAction>>& candidate_plans,
    const std::vector<EmergencyScenario>& scenarios,
    const DispatchConstraints& constraints,
    DispatchAction previous_action) {

    if (candidate_plans.empty() || scenarios.empty() ||
        constraints.maximum_delay_s < 0.0 || constraints.heat_critical < 0.0 ||
        constraints.risk_of_harm_limit < 0.0 ||
        constraints.risk_of_harm_limit > 1.0) {
        throw std::invalid_argument("invalid robust dispatch inputs");
    }

    RobustDispatchResult best;
    best.robust_cost = std::numeric_limits<double>::infinity();

    for (const auto& plan : candidate_plans) {
        if (plan.empty()) continue;
        bool feasible = true;
        double worst_cost = 0.0;
        double max_delay = 0.0;
        double max_heat = 0.0;
        double max_risk = 0.0;

        for (const auto& scenario : scenarios) {
            if (scenario.baseline_delay_s.size() != plan.size() ||
                scenario.baseline_heat_index.size() != plan.size() ||
                scenario.baseline_risk_of_harm.size() != plan.size()) {
                throw std::invalid_argument("scenario horizon differs from candidate plan");
            }

            double scenario_cost = 0.0;
            DispatchAction prior = previous_action;
            for (std::size_t t = 0; t < plan.size(); ++t) {
                const DispatchAction action = plan[t];
                if (action.traffic_priority < 0.0 || action.traffic_priority > 1.0 ||
                    action.cooling_priority < 0.0 || action.cooling_priority > 1.0) {
                    feasible = false;
                    break;
                }

                const double delay = std::max(0.0, scenario.baseline_delay_s[t] -
                    scenario.traffic_delay_reduction_s * action.traffic_priority);
                const double heat = std::max(0.0, scenario.baseline_heat_index[t] -
                    scenario.cooling_heat_reduction * action.cooling_priority);
                const double risk = scenario.baseline_risk_of_harm[t] +
                    scenario.traffic_priority_risk * action.traffic_priority +
                    scenario.cooling_priority_risk * action.cooling_priority;

                if (delay > constraints.maximum_delay_s ||
                    heat >= constraints.heat_critical ||
                    risk > constraints.risk_of_harm_limit) {
                    feasible = false;
                    break;
                }

                const double delta_traffic = action.traffic_priority - prior.traffic_priority;
                const double delta_cooling = action.cooling_priority - prior.cooling_priority;
                scenario_cost += constraints.traffic_weight * delay * delay +
                    constraints.cooling_weight * heat * heat +
                    constraints.change_weight *
                    (delta_traffic * delta_traffic + delta_cooling * delta_cooling);
                prior = action;
                max_delay = std::max(max_delay, delay);
                max_heat = std::max(max_heat, heat);
                max_risk = std::max(max_risk, risk);
            }
            if (!feasible) break;
            worst_cost = std::max(worst_cost, scenario_cost);
        }

        if (feasible && worst_cost < best.robust_cost) {
            const double risk_margin = std::clamp(
                1.0 - max_risk / std::max(1e-9, constraints.risk_of_harm_limit), 0.0, 1.0);
            best = {plan, worst_cost, max_delay, max_heat, max_risk, true,
                    std::clamp(0.55 + 0.45 * risk_margin, 0.0, 1.0),
                    std::clamp(0.50 + 0.50 * risk_margin, 0.0, 1.0)};
        }
    }
    return best;
}

/*
For three water users {agricultural, municipal, ecological}, a payoff vector x
is core-stable iff:
sum_i x_i=v(N), and sum_(i in S)x_i>=v(S) for every nonempty coalition S.

The ecological user is a full coalition member. Its minimum service or flow
value must appear in v(S), rather than being treated as a residual claimant.
*/
struct ThreeUserWaterGame {
    std::array<double, 8> coalition_values{};
};

struct CoreAllocation {
    std::array<double, 3> payoffs{};
    bool stable{};
    double grand_coalition_value{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline bool allocation_in_core(const ThreeUserWaterGame& game,
                               const std::array<double, 3>& allocation,
                               double tolerance = 1e-9) {
    const double grand = game.coalition_values[7];
    const double total = allocation[0] + allocation[1] + allocation[2];
    if (std::abs(total - grand) > tolerance) return false;

    for (std::uint8_t coalition = 1; coalition < 8; ++coalition) {
        double coalition_payoff = 0.0;
        for (std::size_t user = 0; user < 3; ++user) {
            if ((coalition & (std::uint8_t{1} << user)) != 0) {
                coalition_payoff += allocation[user];
            }
        }
        if (coalition_payoff + tolerance < game.coalition_values[coalition]) {
            return false;
        }
    }
    return true;
}

inline CoreAllocation find_three_user_core_allocation(
    const ThreeUserWaterGame& game, double grid_step) {
    if (!(grid_step > 0.0) || game.coalition_values[7] < 0.0) {
        throw std::invalid_argument("invalid cooperative water game");
    }

    const double grand = game.coalition_values[7];
    CoreAllocation best;
    best.grand_coalition_value = grand;
    double best_minimum_payoff = -std::numeric_limits<double>::infinity();

    for (double agricultural = 0.0; agricultural <= grand + 1e-9; agricultural += grid_step) {
        for (double municipal = 0.0; municipal <= grand - agricultural + 1e-9;
             municipal += grid_step) {
            const double ecological = grand - agricultural - municipal;
            const std::array<double, 3> allocation{
                agricultural, municipal, ecological};
            if (!allocation_in_core(game, allocation)) continue;

            const double minimum_payoff = std::min(
                agricultural, std::min(municipal, ecological));
            if (minimum_payoff > best_minimum_payoff) {
                best_minimum_payoff = minimum_payoff;
                best.payoffs = allocation;
                best.stable = true;
            }
        }
    }

    if (best.stable) {
        best.knowledge_factor = std::clamp(
            0.70 + 0.30 * best_minimum_payoff / std::max(1.0, grand), 0.0, 1.0);
        best.eco_impact_value = std::clamp(
            best.payoffs[2] / std::max(1.0, grand), 0.0, 1.0);
    }
    return best;
}

}  // namespace eco_restoration
