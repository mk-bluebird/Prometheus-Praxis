// File: cpp/simulation/irrigation_scenario_diagnostics.hpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace prometheus_praxis::eco_restoration {

struct RainfallScenario {
    double probability{};
    std::vector<double> rainfall;
};

struct IrrigationDynamics {
    double initial_moisture{};
    double minimum_moisture{};
    double maximum_moisture{};
    double evapotranspiration_per_step{};
};

}  // namespace prometheus_praxis::eco_restoration

namespace prometheus_praxis::simulation {

struct IrrigationDryRunStep {
    std::size_t scenario_index{};
    std::size_t horizon_index{};
    double planned_irrigation{};
    double rainfall{};
    double moisture_before{};
    double moisture_after{};
    bool within_bounds{};
};

struct TerminalMoistureScenarioResult {
    std::size_t scenario_index{};
    double probability{};
    double terminal_moisture{};
    bool within_bounds{};
};

struct TerminalMoistureSetAnalysis {
    bool valid{};
    bool all_within_bounds{};
    std::size_t worst_case_scenario_index{};
    double worst_case_terminal_moisture{};
    double minimum_terminal_moisture{};
    double maximum_terminal_moisture{};
    std::vector<TerminalMoistureScenarioResult> scenarios;
    std::vector<std::string> reasons;
};

bool ValidateRainfallScenarioProbabilities(
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    double tolerance,
    std::vector<std::string>* reasons = nullptr);

std::vector<IrrigationDryRunStep> VisualizeIrrigationScheduleDryRun(
    const eco_restoration::IrrigationDynamics& dynamics,
    const std::vector<double>& planned_irrigation,
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    std::vector<std::string>* reasons = nullptr);

TerminalMoistureSetAnalysis AnalyzeTerminalMoistureSet(
    const eco_restoration::IrrigationDynamics& dynamics,
    const std::vector<double>& planned_irrigation,
    const std::vector<eco_restoration::RainfallScenario>& scenarios,
    double probability_tolerance);

std::string ExplainIrrigationScheduleDryRun(
    const std::vector<IrrigationDryRunStep>& steps);

std::string ExplainTerminalMoistureSetAnalysis(
    const TerminalMoistureSetAnalysis& analysis);

bool IrrigationScenarioDiagnosticsSelfTest();

}  // namespace prometheus_praxis::simulation
