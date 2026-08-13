// File: cpp/eco_restoration/irrigation_mpc_and_equitable_water.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

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

inline bool is_finite_nonnegative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

inline void validate_irrigation_dynamics(
    const IrrigationDynamics& dynamics) {
    if (!std::isfinite(dynamics.initial_moisture_mm) ||
        !is_finite_nonnegative(
            dynamics.evapotranspiration_mm_per_step) ||
        !is_finite_nonnegative(dynamics.drainage_fraction) ||
        dynamics.drainage_fraction > 1.0 ||
        !std::isfinite(dynamics.moisture_min_mm) ||
        !std::isfinite(dynamics.moisture_max_mm) ||
        !std::isfinite(dynamics.terminal_min_mm) ||
        !std::isfinite(dynamics.terminal_max_mm) ||
        !is_finite_nonnegative(
            dynamics.irrigation_max_mm_per_step) ||
        !is_finite_nonnegative(dynamics.irrigation_cost) ||
        !is_finite_nonnegative(dynamics.stress_cost) ||
        dynamics.moisture_min_mm > dynamics.moisture_max_mm ||
        dynamics.terminal_min_mm > dynamics.terminal_max_mm ||
        dynamics.terminal_min_mm < dynamics.moisture_min_mm ||
        dynamics.terminal_max_mm > dynamics.moisture_max_mm) {
        throw std::invalid_argument("invalid irrigation dynamics");
    }
}

inline void validate_rainfall_scenarios(
    const std::vector<RainfallScenario>& scenarios,
    std::size_t horizon) {
    if (scenarios.empty() || horizon == 0U) {
        throw std::invalid_argument(
            "irrigation scenarios and horizon must be non-empty");
    }

    double probability_sum = 0.0;

    for (const auto& scenario : scenarios) {
        if (!std::isfinite(scenario.probability) ||
            scenario.probability < 0.0 ||
            scenario.rainfall_mm.size() != horizon) {
            throw std::invalid_argument(
                "invalid rainfall scenario");
        }

        for (const double rainfall_mm : scenario.rainfall_mm) {
            if (!std::isfinite(rainfall_mm) ||
                rainfall_mm < 0.0) {
                throw std::invalid_argument(
                    "rainfall values must be finite and nonnegative");
            }
        }

        probability_sum += scenario.probability;
    }

    constexpr double probability_tolerance = 1e-9;

    if (!std::isfinite(probability_sum) ||
        std::abs(probability_sum - 1.0) >
            probability_tolerance) {
        throw std::invalid_argument(
            "rainfall scenario probabilities must sum to one");
    }
}

inline double next_soil_moisture(
    double moisture_mm,
    double irrigation_mm,
    double rainfall_mm,
    const IrrigationDynamics& dynamics) {
    if (!std::isfinite(moisture_mm) ||
        !std::isfinite(irrigation_mm) ||
        !std::isfinite(rainfall_mm)) {
        throw std::invalid_argument(
            "soil moisture inputs must be finite");
    }

    const double excess = std::max(
        0.0,
        moisture_mm - dynamics.moisture_max_mm);

    const double next_moisture =
        moisture_mm +
        irrigation_mm +
        rainfall_mm -
        dynamics.evapotranspiration_mm_per_step -
        dynamics.drainage_fraction * excess;

    if (!std::isfinite(next_moisture)) {
        throw std::runtime_error(
            "soil moisture update is non-finite");
    }

    return next_moisture;
}

inline IrrigationMpcResult select_robust_irrigation_schedule(
    const std::vector<std::vector<double>>& candidate_schedules,
    const std::vector<RainfallScenario>& scenarios,
    const IrrigationDynamics& dynamics) {
    if (candidate_schedules.empty()) {
        throw std::invalid_argument(
            "candidate irrigation schedules must not be empty");
    }

    validate_irrigation_dynamics(dynamics);

    const std::size_t horizon =
        candidate_schedules.front().size();

    if (horizon == 0U) {
        throw std::invalid_argument(
            "candidate irrigation schedule horizon must be positive");
    }

    validate_rainfall_scenarios(scenarios, horizon);

    IrrigationMpcResult best;
    best.expected_cost = std::numeric_limits<double>::infinity();

    for (const auto& schedule : candidate_schedules) {
        if (schedule.size() != horizon) {
            throw std::invalid_argument(
                "candidate schedules must share one horizon");
        }

        bool feasible = true;
        double expected_cost = 0.0;
        double worst_terminal_moisture =
            std::numeric_limits<double>::infinity();

        for (const auto& scenario : scenarios) {
            double moisture = dynamics.initial_moisture_mm;
            double scenario_cost = 0.0;

            for (std::size_t step = 0U;
                 step < schedule.size();
                 ++step) {
                const double irrigation_mm = schedule[step];

                if (!std::isfinite(irrigation_mm) ||
                    irrigation_mm < 0.0 ||
                    irrigation_mm >
                        dynamics.irrigation_max_mm_per_step) {
                    feasible = false;
                    break;
                }

                const double moisture_deficit = std::max(
                    0.0,
                    dynamics.moisture_min_mm - moisture);

                scenario_cost +=
                    dynamics.irrigation_cost *
                    irrigation_mm *
                    irrigation_mm;

                scenario_cost +=
                    dynamics.stress_cost *
                    moisture_deficit *
                    moisture_deficit;

                moisture = next_soil_moisture(
                    moisture,
                    irrigation_mm,
                    scenario.rainfall_mm[step],
                    dynamics);

                if (moisture < dynamics.moisture_min_mm ||
                    moisture > dynamics.moisture_max_mm) {
                    feasible = false;
                    break;
                }
            }

            if (!feasible ||
                moisture < dynamics.terminal_min_mm ||
                moisture > dynamics.terminal_max_mm) {
                feasible = false;
                break;
            }

            expected_cost +=
                scenario.probability * scenario_cost;

            worst_terminal_moisture = std::min(
                worst_terminal_moisture,
                moisture);
        }

        if (!feasible ||
            !std::isfinite(expected_cost) ||
            !std::isfinite(worst_terminal_moisture) ||
            expected_cost >= best.expected_cost) {
            continue;
        }

        const double water_total = std::accumulate(
            schedule.begin(),
            schedule.end(),
            0.0);

        const double possible_total =
            dynamics.irrigation_max_mm_per_step *
            static_cast<double>(schedule.size());

        const double conservation =
            possible_total > 0.0
                ? 1.0 - water_total / possible_total
                : (water_total == 0.0 ? 1.0 : 0.0);

        best.schedule_mm = schedule;
        best.expected_cost = expected_cost;
        best.worst_case_terminal_moisture_mm =
            worst_terminal_moisture;
        best.robustly_feasible = true;
        best.knowledge_factor = std::clamp(
            0.80 + 0.20 * conservation,
            0.0,
            1.0);
        best.eco_impact_value = std::clamp(
            0.55 + 0.45 * conservation,
            0.0,
            1.0);
    }

    return best;
}

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

inline void validate_water_stakeholder(
    const WaterStakeholder& stakeholder) {
    if (!std::isfinite(stakeholder.minimum_allocation_mm) ||
        !std::isfinite(stakeholder.maximum_allocation_mm) ||
        stakeholder.minimum_allocation_mm < 0.0 ||
        stakeholder.maximum_allocation_mm <
            stakeholder.minimum_allocation_mm ||
        !stakeholder.utility) {
        throw std::invalid_argument(
            "invalid water stakeholder");
    }
}

inline EquitableAllocationResult evaluate_water_allocation(
    const std::vector<double>& allocation_mm,
    const std::vector<WaterStakeholder>& stakeholders,
    double available_water_mm,
    double equity_tolerance) {
    if (stakeholders.empty() ||
        allocation_mm.size() != stakeholders.size() ||
        !is_finite_nonnegative(available_water_mm) ||
        !is_finite_nonnegative(equity_tolerance)) {
        throw std::invalid_argument(
            "invalid water allocation inputs");
    }

    double total_water = 0.0;
    double total_utility = 0.0;
    double minimum_utility =
        std::numeric_limits<double>::infinity();
    double maximum_utility =
        -std::numeric_limits<double>::infinity();

    for (std::size_t index = 0U;
         index < stakeholders.size();
         ++index) {
        const WaterStakeholder& stakeholder =
            stakeholders[index];

        const double allocation = allocation_mm[index];

        validate_water_stakeholder(stakeholder);

        if (!std::isfinite(allocation) ||
            allocation < stakeholder.minimum_allocation_mm ||
            allocation > stakeholder.maximum_allocation_mm) {
            throw std::invalid_argument(
                "allocation violates stakeholder bounds");
        }

        const double utility =
            stakeholder.utility(allocation);

        if (!std::isfinite(utility)) {
            throw std::invalid_argument(
                "stakeholder utility must be finite");
        }

        total_water += allocation;
        total_utility += utility;
        minimum_utility = std::min(
            minimum_utility,
            utility);
        maximum_utility = std::max(
            maximum_utility,
            utility);
    }

    if (!std::isfinite(total_water) ||
        !std::isfinite(total_utility)) {
        throw std::runtime_error(
            "water allocation aggregation is non-finite");
    }

    const double utility_gap =
        maximum_utility - minimum_utility;

    const bool equitable =
        total_water <= available_water_mm &&
        utility_gap <= equity_tolerance;

    const double normalized_gap =
        utility_gap /
        std::max(1.0, equity_tolerance + 1.0);

    const double knowledge_factor = std::clamp(
        1.0 - normalized_gap,
        0.0,
        1.0);

    const double eco_impact_value = equitable
        ? std::clamp(
            total_utility /
                (std::abs(total_utility) + 1.0),
            0.0,
            1.0)
        : 0.0;

    return {
        allocation_mm,
        total_utility,
        utility_gap,
        equitable,
        knowledge_factor,
        eco_impact_value
    };
}

}
