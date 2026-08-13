// File: cpp/eco_restoration/maintenance_and_lifecycle_optimization.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

/*
Binary maintenance formulation:
min sum_(i,t) c_(i,t) x_(i,t)
s.t. s_i(t+1)=clamp(s_i(t)+d_i(t)-m_i*x_(i,t),0,s_i,max),
     reliability_i(t)=exp(-gamma_i*s_i(t)) >= reliability_min_i,
     sum_(i,t)c_(i,t)x_(i,t) <= budget,
     x_(i,t) in {0,1}.

The enumerator below is intended for small, auditable cooling-infrastructure
fleets. Larger instances preserve the same formulation for a MILP solver.
*/
struct CoolingAsset {
    double initial_degradation{};
    double degradation_per_period{};
    double maintenance_reduction{};
    double degradation_maximum{};
    double reliability_decay{};
    double minimum_reliability{};
    std::vector<double> maintenance_cost_by_period;
};

struct MaintenanceScheduleResult {
    std::vector<bool> maintenance_actions;
    double total_cost{};
    double minimum_reliability{};
    bool feasible{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline double reliability_from_degradation(double degradation,
                                           double reliability_decay) {
    return std::exp(-std::max(0.0, degradation) * std::max(0.0, reliability_decay));
}

inline MaintenanceScheduleResult optimize_maintenance_schedule(
    const std::vector<CoolingAsset>& assets, std::size_t periods, double budget) {

    if (assets.empty() || periods == 0 || budget < 0.0) {
        throw std::invalid_argument("invalid maintenance optimization inputs");
    }

    const std::size_t decisions = assets.size() * periods;
    if (decisions > 24) {
        throw std::invalid_argument("enumeration supports at most 24 maintenance decisions");
    }
    for (const auto& asset : assets) {
        if (asset.maintenance_cost_by_period.size() != periods ||
            asset.initial_degradation < 0.0 ||
            asset.degradation_per_period < 0.0 ||
            asset.maintenance_reduction < 0.0 ||
            asset.degradation_maximum < 0.0 ||
            asset.reliability_decay < 0.0 ||
            asset.minimum_reliability < 0.0 ||
            asset.minimum_reliability > 1.0) {
            throw std::invalid_argument("invalid cooling asset parameters");
        }
    }

    MaintenanceScheduleResult best;
    best.total_cost = std::numeric_limits<double>::infinity();
    const std::uint64_t schedules = std::uint64_t{1} << decisions;

    for (std::uint64_t mask = 0; mask < schedules; ++mask) {
        std::vector<double> degradation(assets.size());
        for (std::size_t i = 0; i < assets.size(); ++i) {
            degradation[i] = assets[i].initial_degradation;
        }

        bool feasible = true;
        double cost = 0.0;
        double minimum_reliability = 1.0;
        std::vector<bool> actions(decisions, false);

        for (std::size_t t = 0; t < periods && feasible; ++t) {
            for (std::size_t i = 0; i < assets.size(); ++i) {
                const std::size_t index = t * assets.size() + i;
                const bool maintain = (mask & (std::uint64_t{1} << index)) != 0;
                actions[index] = maintain;
                if (maintain) cost += assets[i].maintenance_cost_by_period[t];
                if (cost > budget) {
                    feasible = false;
                    break;
                }

                degradation[i] = std::clamp(
                    degradation[i] + assets[i].degradation_per_period -
                    (maintain ? assets[i].maintenance_reduction : 0.0),
                    0.0, assets[i].degradation_maximum);
                const double reliability = reliability_from_degradation(
                    degradation[i], assets[i].reliability_decay);
                minimum_reliability = std::min(minimum_reliability, reliability);
                if (reliability < assets[i].minimum_reliability) {
                    feasible = false;
                    break;
                }
            }
        }

        if (feasible && cost < best.total_cost) {
            const double budget_margin = std::clamp(
                1.0 - cost / std::max(budget, 1e-9), 0.0, 1.0);
            best = {actions, cost, minimum_reliability, true,
                    std::clamp(0.65 * minimum_reliability + 0.35 * budget_margin, 0.0, 1.0),
                    std::clamp(0.75 * minimum_reliability + 0.25 * budget_margin, 0.0, 1.0)};
        }
    }
    return best;
}

/*
Lifecycle investment formulation:
max_a G(a)-lambda*L(a)
s.t. a is safe and every non-offsettable ecological corridor is satisfied.

An action is Pareto-optimal when no other safe action has both:
G(other)>=G(action), L(other)<=L(action),
with at least one strict inequality.

The investment question is: which Pareto-optimal action remains acceptable as
lambda varies, while retaining every non-offsettable corridor constraint?
*/
struct LifecycleAction {
    std::string identifier;
    double lifecycle_emissions_kgco2e{};
    double eco_value{};
    bool safe{};
    bool non_offsettable_corridors_satisfied{};
};

struct LifecycleSelection {
    std::vector<LifecycleAction> pareto_actions;
    LifecycleAction scalarized_choice;
    bool has_scalarized_choice{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline bool lifecycle_dominates(const LifecycleAction& left,
                                const LifecycleAction& right) {
    return left.eco_value >= right.eco_value &&
           left.lifecycle_emissions_kgco2e <= right.lifecycle_emissions_kgco2e &&
           (left.eco_value > right.eco_value ||
            left.lifecycle_emissions_kgco2e < right.lifecycle_emissions_kgco2e);
}

inline LifecycleSelection select_lifecycle_investments(
    const std::vector<LifecycleAction>& actions, double lifecycle_weight) {
    if (actions.empty() || lifecycle_weight < 0.0) {
        throw std::invalid_argument("invalid lifecycle investment inputs");
    }

    std::vector<LifecycleAction> eligible;
    for (const auto& action : actions) {
        if (action.identifier.empty() || action.lifecycle_emissions_kgco2e < 0.0) {
            throw std::invalid_argument("invalid lifecycle action");
        }
        if (action.safe && action.non_offsettable_corridors_satisfied) {
            eligible.push_back(action);
        }
    }

    std::vector<LifecycleAction> pareto;
    for (const auto& action : eligible) {
        bool dominated = false;
        for (const auto& alternative : eligible) {
            if (alternative.identifier != action.identifier &&
                lifecycle_dominates(alternative, action)) {
                dominated = true;
                break;
            }
        }
        if (!dominated) pareto.push_back(action);
    }

    LifecycleSelection result;
    result.pareto_actions = pareto;
    double best_objective = -std::numeric_limits<double>::infinity();
    for (const auto& action : pareto) {
        const double objective = action.eco_value -
            lifecycle_weight * action.lifecycle_emissions_kgco2e;
        if (objective > best_objective) {
            best_objective = objective;
            result.scalarized_choice = action;
            result.has_scalarized_choice = true;
        }
    }

    if (result.has_scalarized_choice) {
        const double normalized_value = std::clamp(
            result.scalarized_choice.eco_value /
            (std::abs(result.scalarized_choice.eco_value) + 1.0), 0.0, 1.0);
        result.knowledge_factor = std::clamp(
            0.60 + 0.40 / static_cast<double>(std::max<std::size_t>(1, pareto.size())),
            0.0, 1.0);
        result.eco_impact_value = normalized_value;
    }
    return result;
}

}  // namespace eco_restoration
