// File: cpp/tools/foundation_orchestrator.hpp
#pragma once

#include "foundation_report.hpp"
#include "../eco_restoration/invasive_control_diagnostics.hpp"
#include "../eco_restoration/water_biodiversity_diagnostics.hpp"
#include "../simulation/irrigation_scenario_diagnostics.hpp"
#include "proof_checked_dispatch_replay.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace prometheus_praxis::eco_restoration {

struct PrivateHeatStatement {
    bool accepted{};
    double risk_of_harm{};
};

struct ThreatAssessment {
    bool fail_closed{};
    double risk_of_harm{};
};

struct WaterAllocation {
    std::int64_t available_water_ml{};
    std::int64_t ecological_reserve_ml{};
    std::int64_t allocated_water_ml{};
    bool required_cross_shard_unsat{};
};

struct BiodiversityIndex {
    double value{};
    double minimum_required{};
};

struct AuthorizationEvidence {
    bool verified{};
    bool policy_matches{};
    bool active{};
    double risk_of_harm{};
};

struct StochasticPopulationModel {
    double baseline_abundance{};
    double projected_abundance{};
    double risk_of_harm{};
};

}  // namespace prometheus_praxis::eco_restoration

namespace prometheus_praxis::foundation::orchestration {

struct FoundationOrchestrationInputs {
    eco_restoration::PrivateHeatStatement private_heat;
    eco_restoration::ThreatAssessment threat;
    eco_restoration::WaterAllocation water;
    eco_restoration::BiodiversityIndex biodiversity;
    eco_restoration::AuthorizationEvidence authorization;
    eco_restoration::StochasticPopulationModel population_model;
    std::vector<eco_restoration::InvasiveControlCandidate> invasive_candidates;
    eco_restoration::IrrigationDynamics irrigation_dynamics;
    std::vector<eco_restoration::RainfallScenario> rainfall_scenarios;
    std::vector<double> irrigation_schedule;
};

struct FoundationOrchestrationResult {
    bool valid{};
    FoundationReport report;
    std::vector<std::string> failure_reasons;
};

FoundationOrchestrationResult RunFoundationSelfCheck(
    const FoundationOrchestrationInputs& inputs);

FoundationReport ComposeFoundationReport(
    bool private_heat_accepted,
    bool threat_fail_closed,
    bool water_allowed,
    bool water_invariant,
    bool authorization_accepted,
    bool invasive_safe,
    bool irrigation_feasible,
    double maximum_risk,
    double knowledge_factor,
    double eco_impact_value);

bool IsOrchestrationResultConsistent(
    const FoundationOrchestrationResult& result);

std::string ExplainOrchestrationFailure(
    const FoundationOrchestrationResult& result);

bool FoundationSelfCheckOrchestratorSelfTest();

}  // namespace prometheus_praxis::foundation::orchestration
