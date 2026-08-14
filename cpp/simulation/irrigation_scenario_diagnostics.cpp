// File: cpp/simulation/irrigation_scenario_diagnostics.cpp
#include "irrigation_scenario_diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace prometheus_praxis::simulation {
namespace {

bool IsFinite(double value) noexcept {
    return std::isfinite(value);
}

bool IsFiniteNonNegative(double value) noexcept {
    return IsFinite(value) && value >= 0.0;
}

void AddReason(std::vector<std::string>* reasons, std::string reason) {
    if (reasons != nullptr) {
        reasons->push_back(std::move(reason));
    }
}

std::string FormatDouble(double value) {
    if (std::isnan(value)) {
        return "nan";
    }
    if (std::isinf(value)) {
        return value > 0.0 ? "infinity" : "-infinity";
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6) << value;
    return output.str();
}

bool IsValidProbabilityTolerance(double tolerance) noexcept {
    return IsFinite(tolerance) && tolerance >= 0.0 && tolerance <= 1.0;
}

bool IsValidDynamics(
    const eco_restoration::IrrigationDynamics& dynamics,
    std::vector<std::string>* reasons) {
    const bool valid =
        IsFinite(dynamics.initial_moisture) &&
        IsFinite(dynamics.minimum_moisture) &&
        IsFinite(dynamics.maximum_moisture) &&
        IsFinite(dynamics.evapotranspiration_per_step) &&
        dynamics.minimum_moisture <= dynamics.maximum_moisture &&
        dynamics.initial_moisture >= dynamics.minimum_moisture &&
        dynamics.initial_moisture <= dynamics.maximum_moisture &&
        dynamics.evapotranspiration_per_step >= 0.0;

    if (!valid) {
        AddReason(
            reasons,
            "irrigation dynamics require finite values, initial moisture within "
            "configured bounds, ordered bounds, and non-negative evapotranspiration");
    }

    return valid;
}

bool IsWithinBounds(
    double moisture,
    const eco_restoration::IrrigationDynamics& dynamics) noexcept {
    return IsFinite(moisture) &&
           moisture >= dynamics.minimum_moisture &&
           moisture <= dynamics.maximum_moisture;
}

bool ValidateSchedule(
    const std::vector<double>& planned_irrigation,
    std::vector<std::string>* reasons) {
    for (std::size_t horizon = 0U; horizon < planned_irrigation.size(); ++horizon) {
        if (!IsFiniteNonNegative(planned_irrigation[horizon])) {
            AddReason(
                reasons,
                "planned irrigation at horizon " + std::to_string(horizon) +
                    " must be finite and non-negative");
            return false;
        }
    }
    return true;
}

bool ValidateScenarioHorizonsAndRainfall(
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::size_t schedule_horizon,
    std::vector<std::string>* reasons) {
    for (std::size_t scenario_index = 0U;
         scenario_index < scenarios.size();
         ++scenario_index) {
        const eco_restoration::RainfallScenario& scenario =
            scenarios[scenario_index];

        if (scenario.rainfall.size() != schedule_horizon) {
            AddReason(
                reasons,
                "scenario " + std::to_string(scenario_index) +
                    " rainfall horizon does not match irrigation schedule");
            return false;
        }

        for (std::size_t horizon = 0U;
             horizon < scenario.rainfall.size();
             ++horizon) {
            if (!IsFiniteNonNegative(scenario.rainfall[horizon])) {
                AddReason(
                    reasons,
                    "scenario " + std::to_string(scenario_index) +
                        " has non-finite or negative rainfall at horizon " +
                        std::to_string(horizon));
                return false;
            }
        }
    }
    return true;
}

bool IsInputSetValid(
    const eco_restoration::IrrigationDynamics& dynamics,
    const std::vector<double>& planned_irrigation,
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::vector<std::string>* reasons) {
    return IsValidDynamics(dynamics, reasons) &&
           ValidateSchedule(planned_irrigation, reasons) &&
           ValidateScenarioHorizonsAndRainfall(
               scenarios,
               planned_irrigation.size(),
               reasons);
}

}  // namespace

bool ValidateRainfallScenarioProbabilities(
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    double tolerance,
    std::vector<std::string>* reasons) {
    if (!IsValidProbabilityTolerance(tolerance)) {
        AddReason(
            reasons,
            "probability tolerance must be finite and within [0,1]");
        return false;
    }

    if (scenarios.empty()) {
        AddReason(reasons, "at least one rainfall scenario is required");
        return false;
    }

    double sum = 0.0;
    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        const double probability = scenarios[index].probability;
        if (!IsFinite(probability) || probability < 0.0 || probability > 1.0) {
            AddReason(
                reasons,
                "scenario " + std::to_string(index) +
                    " probability must be finite and within [0,1]");
            return false;
        }

        sum += probability;
        if (!IsFinite(sum)) {
            AddReason(reasons, "rainfall probability sum is non-finite");
            return false;
        }
    }

    if (std::fabs(sum - 1.0) > tolerance) {
        AddReason(
            reasons,
            "rainfall probabilities sum to " + FormatDouble(sum) +
                " instead of one within tolerance " + FormatDouble(tolerance));
        return false;
    }

    return true;
}

std::vector<IrrigationDryRunStep> VisualizeIrrigationScheduleDryRun(
    const eco_restoration::IrrigationDynamics& dynamics,
    const std::vector<double>& planned_irrigation,
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::vector<std::string>* reasons) {
    std::vector<IrrigationDryRunStep> steps;

    if (!IsInputSetValid(dynamics, planned_irrigation, scenarios, reasons)) {
        return steps;
    }

    if (scenarios.empty()) {
        AddReason(reasons, "at least one rainfall scenario is required");
        return steps;
    }

    if (planned_irrigation.empty()) {
        return steps;
    }

    steps.reserve(scenarios.size() * planned_irrigation.size());

    for (std::size_t scenario_index = 0U;
         scenario_index < scenarios.size();
         ++scenario_index) {
        double moisture = dynamics.initial_moisture;
        const eco_restoration::RainfallScenario& scenario =
            scenarios[scenario_index];

        for (std::size_t horizon = 0U;
             horizon < planned_irrigation.size();
             ++horizon) {
            const double moisture_before = moisture;
            const double moisture_after =
                moisture_before + planned_irrigation[horizon] +
                scenario.rainfall[horizon] -
                dynamics.evapotranspiration_per_step;

            if (!IsFinite(moisture_after)) {
                AddReason(
                    reasons,
                    "moisture transition is non-finite at scenario " +
                        std::to_string(scenario_index) +
                        ", horizon " + std::to_string(horizon));
                return {};
            }

            steps.push_back(IrrigationDryRunStep{
                scenario_index,
                horizon,
                planned_irrigation[horizon],
                scenario.rainfall[horizon],
                moisture_before,
                moisture_after,
                IsWithinBounds(moisture_after, dynamics)});

            moisture = moisture_after;
        }
    }

    return steps;
}

TerminalMoistureSetAnalysis AnalyzeTerminalMoistureSet(
    const eco_restoration::IrrigationDynamics& dynamics,
    const std::vector<double>& planned_irrigation,
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    double probability_tolerance) {
    TerminalMoistureSetAnalysis analysis;
    analysis.worst_case_terminal_moisture =
        std::numeric_limits<double>::quiet_NaN();
    analysis.minimum_terminal_moisture =
        std::numeric_limits<double>::quiet_NaN();
    analysis.maximum_terminal_moisture =
        std::numeric_limits<double>::quiet_NaN();

    if (!ValidateRainfallScenarioProbabilities(
            scenarios,
            probability_tolerance,
            &analysis.reasons) ||
        !IsInputSetValid(
            dynamics,
            planned_irrigation,
            scenarios,
            &analysis.reasons)) {
        return analysis;
    }

    if (planned_irrigation.empty()) {
        analysis.scenarios.reserve(scenarios.size());
        for (std::size_t scenario_index = 0U;
             scenario_index < scenarios.size();
             ++scenario_index) {
            analysis.scenarios.push_back(TerminalMoistureScenarioResult{
                scenario_index,
                scenarios[scenario_index].probability,
                dynamics.initial_moisture,
                IsWithinBounds(dynamics.initial_moisture, dynamics)});
        }
    } else {
        const std::vector<IrrigationDryRunStep> steps =
            VisualizeIrrigationScheduleDryRun(
                dynamics,
                planned_irrigation,
                scenarios,
                &analysis.reasons);
        if (!analysis.reasons.empty() ||
            steps.size() != scenarios.size() * planned_irrigation.size()) {
            if (analysis.reasons.empty()) {
                analysis.reasons.emplace_back(
                    "dry run did not produce a complete scenario trajectory set");
            }
            return analysis;
        }

        analysis.scenarios.reserve(scenarios.size());
        for (std::size_t scenario_index = 0U;
             scenario_index < scenarios.size();
             ++scenario_index) {
            const std::size_t terminal_index =
                ((scenario_index + 1U) * planned_irrigation.size()) - 1U;
            const IrrigationDryRunStep& terminal = steps[terminal_index];

            analysis.scenarios.push_back(TerminalMoistureScenarioResult{
                scenario_index,
                scenarios[scenario_index].probability,
                terminal.moisture_after,
                terminal.within_bounds});
        }
    }

    if (analysis.scenarios.empty()) {
        analysis.reasons.emplace_back("no terminal moisture scenarios were produced");
        return analysis;
    }

    analysis.valid = true;
    analysis.all_within_bounds = true;
    analysis.worst_case_scenario_index = analysis.scenarios.front().scenario_index;
    analysis.worst_case_terminal_moisture =
        analysis.scenarios.front().terminal_moisture;
    analysis.minimum_terminal_moisture =
        analysis.scenarios.front().terminal_moisture;
    analysis.maximum_terminal_moisture =
        analysis.scenarios.front().terminal_moisture;

    for (const TerminalMoistureScenarioResult& scenario : analysis.scenarios) {
        if (!scenario.within_bounds) {
            analysis.all_within_bounds = false;
            analysis.reasons.emplace_back(
                "scenario " + std::to_string(scenario.scenario_index) +
                " terminal moisture is outside configured bounds");
        }

        if (scenario.terminal_moisture < analysis.worst_case_terminal_moisture) {
            analysis.worst_case_terminal_moisture = scenario.terminal_moisture;
            analysis.worst_case_scenario_index = scenario.scenario_index;
        }

        analysis.minimum_terminal_moisture = std::min(
            analysis.minimum_terminal_moisture,
            scenario.terminal_moisture);
        analysis.maximum_terminal_moisture = std::max(
            analysis.maximum_terminal_moisture,
            scenario.terminal_moisture);
    }

    return analysis;
}

std::string ExplainIrrigationScheduleDryRun(
    const std::vector<IrrigationDryRunStep>& steps) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "irrigation_dry_run; step_count=" << steps.size();

    for (const IrrigationDryRunStep& step : steps) {
        output << "; scenario=" << step.scenario_index
               << "; horizon=" << step.horizon_index
               << "; planned_irrigation=" << FormatDouble(step.planned_irrigation)
               << "; rainfall=" << FormatDouble(step.rainfall)
               << "; moisture_before=" << FormatDouble(step.moisture_before)
               << "; moisture_after=" << FormatDouble(step.moisture_after)
               << "; within_bounds="
               << (step.within_bounds ? "true" : "false");
    }

    return output.str();
}

std::string ExplainTerminalMoistureSetAnalysis(
    const TerminalMoistureSetAnalysis& analysis) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "terminal_moisture_analysis"
           << "; valid=" << (analysis.valid ? "true" : "false")
           << "; all_within_bounds="
           << (analysis.all_within_bounds ? "true" : "false")
           << "; worst_case_scenario=" << analysis.worst_case_scenario_index
           << "; worst_case_terminal_moisture="
           << FormatDouble(analysis.worst_case_terminal_moisture)
           << "; minimum_terminal_moisture="
           << FormatDouble(analysis.minimum_terminal_moisture)
           << "; maximum_terminal_moisture="
           << FormatDouble(analysis.maximum_terminal_moisture);

    for (const TerminalMoistureScenarioResult& scenario : analysis.scenarios) {
        output << "; scenario=" << scenario.scenario_index
               << "; probability=" << FormatDouble(scenario.probability)
               << "; terminal_moisture="
               << FormatDouble(scenario.terminal_moisture)
               << "; within_bounds="
               << (scenario.within_bounds ? "true" : "false");
    }

    for (const std::string& reason : analysis.reasons) {
        output << "; reason=" << reason;
    }

    return output.str();
}

bool IrrigationScenarioDiagnosticsSelfTest() {
    const eco_restoration::IrrigationDynamics dynamics{
        0.50,
        0.20,
        0.90,
        0.10};

    const std::vector<double> feasible_schedule{0.10, 0.10};
    const std::vector<eco_restoration::RainfallScenario> feasible_scenarios{
        eco_restoration::RainfallScenario{0.50, {0.10, 0.10}},
        eco_restoration::RainfallScenario{0.50, {0.00, 0.05}}};

    const TerminalMoistureSetAnalysis feasible =
        AnalyzeTerminalMoistureSet(
            dynamics,
            feasible_schedule,
            feasible_scenarios,
            0.000001);
    if (!feasible.valid ||
        !feasible.all_within_bounds ||
        feasible.worst_case_scenario_index != 1U ||
        std::fabs(feasible.worst_case_terminal_moisture - 0.55) > 0.000001) {
        return false;
    }

    const std::vector<double> infeasible_schedule{0.00, 0.00};
    const std::vector<eco_restoration::RainfallScenario> drought_scenarios{
        eco_restoration::RainfallScenario{0.50, {0.00, 0.00}},
        eco_restoration::RainfallScenario{0.50, {0.00, 0.05}}};
    const TerminalMoistureSetAnalysis infeasible =
        AnalyzeTerminalMoistureSet(
            dynamics,
            infeasible_schedule,
            drought_scenarios,
            0.000001);
    if (!infeasible.valid ||
        infeasible.all_within_bounds ||
        infeasible.worst_case_scenario_index != 0U ||
        std::fabs(infeasible.worst_case_terminal_moisture - 0.30) > 0.000001) {
        return false;
    }

    const std::vector<eco_restoration::RainfallScenario> mismatched_horizon{
        eco_restoration::RainfallScenario{1.0, {0.10}}};
    const TerminalMoistureSetAnalysis mismatch =
        AnalyzeTerminalMoistureSet(
            dynamics,
            feasible_schedule,
            mismatched_horizon,
            0.000001);
    if (mismatch.valid || mismatch.reasons.empty()) {
        return false;
    }

    const std::vector<eco_restoration::RainfallScenario> invalid_probability{
        eco_restoration::RainfallScenario{0.40, {0.0, 0.0}},
        eco_restoration::RainfallScenario{0.40, {0.0, 0.0}}};
    if (ValidateRainfallScenarioProbabilities(
            invalid_probability,
            0.000001,
            nullptr)) {
        return false;
    }

    const std::vector<eco_restoration::RainfallScenario> non_finite_rainfall{
        eco_restoration::RainfallScenario{
            1.0,
            {0.0, std::numeric_limits<double>::quiet_NaN()}}};
    const TerminalMoistureSetAnalysis non_finite =
        AnalyzeTerminalMoistureSet(
            dynamics,
            feasible_schedule,
            non_finite_rainfall,
            0.000001);
    if (non_finite.valid || non_finite.reasons.empty()) {
        return false;
    }

    const std::vector<IrrigationDryRunStep> dry_run =
        VisualizeIrrigationScheduleDryRun(
            dynamics,
            feasible_schedule,
            feasible_scenarios,
            nullptr);
    const std::string explanation = ExplainIrrigationScheduleDryRun(dry_run);
    return explanation.find("irrigation_dry_run; step_count=4") == 0U &&
           ExplainTerminalMoistureSetAnalysis(feasible).find(
               "worst_case_scenario=1") != std::string::npos;
}

}  // namespace prometheus_praxis::simulation
