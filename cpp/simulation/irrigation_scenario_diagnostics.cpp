// File: cpp/simulation/irrigation_scenario_diagnostics.cpp
#include "irrigation_scenario_diagnostics.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool IsFinite(const double value) noexcept {
    return std::isfinite(value);
}

bool IsFiniteNonNegative(const double value) noexcept {
    return IsFinite(value) && value >= 0.0;
}

bool IsDynamicsValid(
    const eco_restoration::IrrigationDynamics& dynamics) noexcept {
    return IsFinite(dynamics.initial_moisture) &&
           IsFinite(dynamics.minimum_moisture) &&
           IsFinite(dynamics.maximum_moisture) &&
           IsFinite(dynamics.retention_factor) &&
           dynamics.minimum_moisture <= dynamics.maximum_moisture &&
           dynamics.initial_moisture >= dynamics.minimum_moisture &&
           dynamics.initial_moisture <= dynamics.maximum_moisture &&
           dynamics.retention_factor >= 0.0 &&
           dynamics.retention_factor <= 1.0;
}

bool IsScheduleValid(const std::vector<double>& planned_irrigation) noexcept {
    for (const double irrigation : planned_irrigation) {
        if (!IsFiniteNonNegative(irrigation)) {
            return false;
        }
    }
    return true;
}

bool HorizonMatches(
    const std::vector<double>& planned_irrigation,
    const std::vector<eco_restoration::RainfallScenario>& scenarios) noexcept {
    if (planned_irrigation.empty() || scenarios.empty()) {
        return false;
    }

    for (const eco_restoration::RainfallScenario& scenario : scenarios) {
        if (scenario.rainfall_by_horizon.size() != planned_irrigation.size()) {
            return false;
        }
    }
    return true;
}

double AdvanceMoisture(
    const eco_restoration::IrrigationDynamics& dynamics,
    const double moisture_before,
    const double planned_irrigation,
    const double rainfall) noexcept {
    return dynamics.retention_factor * moisture_before +
           planned_irrigation + rainfall;
}

void AddReason(std::vector<std::string>* reasons, const std::string& reason) {
    if (reasons != nullptr) {
        reasons->push_back(reason);
    }
}

}  // namespace

bool ValidateRainfallScenarioProbabilities(
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    const double tolerance,
    std::vector<std::string>* reasons) {
    if (reasons != nullptr) {
        reasons->clear();
    }

    if (!IsFinite(tolerance) || tolerance < 0.0) {
        AddReason(reasons, "probability tolerance must be finite and non-negative");
        return false;
    }

    if (scenarios.empty()) {
        AddReason(reasons, "at least one rainfall scenario is required");
        return false;
    }

    double probability_sum = 0.0;
    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        const double probability = scenarios[index].probability;
        if (!IsFinite(probability) || probability < 0.0 || probability > 1.0) {
            AddReason(
                reasons,
                "rainfall scenario probability must be finite and in [0,1]");
            return false;
        }
        probability_sum += probability;
    }

    if (!IsFinite(probability_sum) ||
        std::fabs(probability_sum - 1.0) > tolerance) {
        AddReason(
            reasons,
            "rainfall scenario probabilities must sum to one within tolerance");
        return false;
    }

    return true;
}

std::vector<IrrigationDryRunStep> VisualizeIrrigationScheduleDryRun(
    const eco_restoration::IrrigationDynamics& dynamics,
    const std::vector<double>& planned_irrigation,
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::vector<std::string>* reasons) {
    if (reasons != nullptr) {
        reasons->clear();
    }

    std::vector<IrrigationDryRunStep> steps;

    if (!IsDynamicsValid(dynamics)) {
        AddReason(reasons, "irrigation dynamics are structurally invalid");
        return steps;
    }

    if (!IsScheduleValid(planned_irrigation)) {
        AddReason(reasons, "planned irrigation must contain finite non-negative values");
        return steps;
    }

    if (!HorizonMatches(planned_irrigation, scenarios)) {
        AddReason(
            reasons,
            "every rainfall scenario horizon must match the irrigation schedule");
        return steps;
    }

    for (std::size_t scenario_index = 0U;
         scenario_index < scenarios.size();
         ++scenario_index) {
        double moisture = dynamics.initial_moisture;

        for (std::size_t horizon_index = 0U;
             horizon_index < planned_irrigation.size();
             ++horizon_index) {
            const double rainfall =
                scenarios[scenario_index].rainfall_by_horizon[horizon_index];

            if (!IsFiniteNonNegative(rainfall)) {
                AddReason(
                    reasons,
                    "rainfall amounts must be finite non-negative values");
                return {};
            }

            const double moisture_after = AdvanceMoisture(
                dynamics,
                moisture,
                planned_irrigation[horizon_index],
                rainfall);

            if (!IsFinite(moisture_after)) {
                AddReason(reasons, "moisture transition produced a non-finite value");
                return {};
            }

            steps.push_back(
                IrrigationDryRunStep{
                    scenario_index,
                    horizon_index,
                    planned_irrigation[horizon_index],
                    rainfall,
                    moisture,
                    moisture_after,
                    moisture_after >= dynamics.minimum_moisture &&
                        moisture_after <= dynamics.maximum_moisture});

            moisture = moisture_after;
        }
    }

    return steps;
}

TerminalMoistureSetAnalysis AnalyzeTerminalMoistureSet(
    const eco_restoration::IrrigationDynamics& dynamics,
    const std::vector<double>& planned_irrigation,
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    const double probability_tolerance) {
    TerminalMoistureSetAnalysis result{};
    std::vector<std::string> probability_reasons;

    if (!ValidateRainfallScenarioProbabilities(
            scenarios,
            probability_tolerance,
            &probability_reasons)) {
        result.reasons = std::move(probability_reasons);
        return result;
    }

    std::vector<std::string> dry_run_reasons;
    const std::vector<IrrigationDryRunStep> steps =
        VisualizeIrrigationScheduleDryRun(
            dynamics,
            planned_irrigation,
            scenarios,
            &dry_run_reasons);

    if (!dry_run_reasons.empty()) {
        result.reasons = std::move(dry_run_reasons);
        return result;
    }

    if (steps.empty()) {
        result.reasons.emplace_back("dry run produced no moisture transitions");
        return result;
    }

    result.valid = true;
    result.all_within_bounds = true;
    result.minimum_terminal_moisture =
        std::numeric_limits<double>::infinity();
    result.maximum_terminal_moisture =
        -std::numeric_limits<double>::infinity();
    result.worst_case_terminal_moisture =
        std::numeric_limits<double>::infinity();

    for (std::size_t scenario_index = 0U;
         scenario_index < scenarios.size();
         ++scenario_index) {
        const std::size_t final_step_index =
            (scenario_index + 1U) * planned_irrigation.size() - 1U;
        const IrrigationDryRunStep& final_step = steps[final_step_index];

        const bool terminal_within_bounds =
            final_step.moisture_after >= dynamics.minimum_moisture &&
            final_step.moisture_after <= dynamics.maximum_moisture;

        result.scenarios.push_back(
            eco_restoration::TerminalMoistureScenarioResult{
                scenario_index,
                scenarios[scenario_index].probability,
                final_step.moisture_after,
                terminal_within_bounds});

        result.all_within_bounds =
            result.all_within_bounds && terminal_within_bounds;

        if (final_step.moisture_after < result.worst_case_terminal_moisture) {
            result.worst_case_terminal_moisture = final_step.moisture_after;
            result.worst_case_scenario_index = scenario_index;
        }

        if (final_step.moisture_after < result.minimum_terminal_moisture) {
            result.minimum_terminal_moisture = final_step.moisture_after;
        }
        if (final_step.moisture_after > result.maximum_terminal_moisture) {
            result.maximum_terminal_moisture = final_step.moisture_after;
        }
    }

    if (!result.all_within_bounds) {
        result.reasons.emplace_back(
            "at least one terminal moisture state is outside configured bounds");
    }

    return result;
}

std::string ExplainIrrigationScheduleDryRun(
    const std::vector<IrrigationDryRunStep>& steps) {
    std::ostringstream output;
    output << "irrigation_dry_run_steps=" << steps.size();

    for (const IrrigationDryRunStep& step : steps) {
        output << '\n'
               << "scenario=" << step.scenario_index
               << ", horizon=" << step.horizon_index
               << ", irrigation=" << step.planned_irrigation
               << ", rainfall=" << step.rainfall
               << ", moisture_before=" << step.moisture_before
               << ", moisture_after=" << step.moisture_after
               << ", within_bounds=" << (step.within_bounds ? "1" : "0");
    }

    return output.str();
}

bool IrrigationScenarioDiagnosticsSelfTest() {
    const eco_restoration::IrrigationDynamics dynamics{
        0.50, 0.20, 1.00, 0.80};

    const std::vector<double> schedule{0.10, 0.10};
    const std::vector<eco_restoration::RainfallScenario> feasible_scenarios{
        eco_restoration::RainfallScenario{0.50, {0.05, 0.05}},
        eco_restoration::RainfallScenario{0.50, {0.00, 0.05}}};

    const TerminalMoistureSetAnalysis feasible =
        AnalyzeTerminalMoistureSet(
            dynamics,
            schedule,
            feasible_scenarios,
            1.0e-9);

    if (!feasible.valid ||
        !feasible.all_within_bounds ||
        feasible.scenarios.size() != 2U ||
        feasible.worst_case_scenario_index != 1U ||
        feasible.worst_case_terminal_moisture !=
            feasible.minimum_terminal_moisture) {
        return false;
    }

    const std::vector<double> infeasible_schedule{0.50, 0.50};
    const TerminalMoistureSetAnalysis infeasible =
        AnalyzeTerminalMoistureSet(
            dynamics,
            infeasible_schedule,
            feasible_scenarios,
            1.0e-9);

    if (!infeasible.valid || infeasible.all_within_bounds) {
        return false;
    }

    const std::vector<eco_restoration::RainfallScenario> mismatched_horizon{
        eco_restoration::RainfallScenario{1.00, {0.05}}};

    const TerminalMoistureSetAnalysis mismatch =
        AnalyzeTerminalMoistureSet(
            dynamics,
            schedule,
            mismatched_horizon,
            1.0e-9);

    if (mismatch.valid) {
        return false;
    }

    const std::vector<eco_restoration::RainfallScenario> invalid_probability{
        eco_restoration::RainfallScenario{0.80, {0.05, 0.05}},
        eco_restoration::RainfallScenario{0.10, {0.00, 0.05}}};

    const TerminalMoistureSetAnalysis invalid =
        AnalyzeTerminalMoistureSet(
            dynamics,
            schedule,
            invalid_probability,
            1.0e-9);

    if (invalid.valid) {
        return false;
    }

    const std::vector<IrrigationDryRunStep> steps =
        VisualizeIrrigationScheduleDryRun(
            dynamics,
            schedule,
            feasible_scenarios);

    return !steps.empty() &&
           ExplainIrrigationScheduleDryRun(steps).find(
               "irrigation_dry_run_steps=4") == 0U;
}
