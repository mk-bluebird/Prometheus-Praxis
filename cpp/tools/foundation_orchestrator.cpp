// File: cpp/tools/foundation_orchestrator.cpp
#include "foundation_orchestrator.hpp"

#include "foundation_report.hpp"
#include "../eco_restoration/invasive_control_diagnostics.hpp"
#include "../eco_restoration/water_biodiversity_diagnostics.hpp"
#include "../simulation/irrigation_scenario_diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace prometheus_praxis::foundation::orchestration {
namespace {

constexpr double kMaximumSafeRisk = 0.30;

bool IsUnitInterval(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

double ClampUnitInterval(double value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 1.0);
}

void AppendReasons(
    std::vector<std::string>& destination,
    const std::vector<std::string>& source,
    const std::string& prefix) {
    for (const std::string& reason : source) {
        destination.push_back(prefix + reason);
    }
}

bool IsValidPopulationModel(
    const eco_restoration::StochasticPopulationModel& population) noexcept {
    return std::isfinite(population.baseline_abundance) &&
           std::isfinite(population.projected_abundance) &&
           std::isfinite(population.risk_of_harm) &&
           population.baseline_abundance >= 0.0 &&
           population.projected_abundance >= 0.0 &&
           IsUnitInterval(population.risk_of_harm);
}

bool IsValidAuthorization(
    const eco_restoration::AuthorizationEvidence& authorization) noexcept {
    return IsUnitInterval(authorization.risk_of_harm);
}

bool IsValidPrivateHeat(
    const eco_restoration::PrivateHeatStatement& private_heat) noexcept {
    return IsUnitInterval(private_heat.risk_of_harm);
}

bool IsValidThreat(
    const eco_restoration::ThreatAssessment& threat) noexcept {
    return IsUnitInterval(threat.risk_of_harm);
}

bool IsValidBiodiversity(
    const eco_restoration::BiodiversityIndex& biodiversity) noexcept {
    return IsUnitInterval(biodiversity.value) &&
           IsUnitInterval(biodiversity.minimum_required);
}

bool IsValidInvasiveCandidate(
    const eco_restoration::InvasiveControlCandidate& candidate) noexcept {
    return std::isfinite(candidate.expected_benefit) &&
           std::isfinite(candidate.treatment_cost) &&
           std::isfinite(candidate.risk_of_harm) &&
           std::isfinite(candidate.expected_next_abundance) &&
           candidate.expected_benefit >= 0.0 &&
           candidate.treatment_cost >= 0.0 &&
           candidate.risk_of_harm >= 0.0 &&
           candidate.risk_of_harm <= 1.0 &&
           candidate.expected_next_abundance >= 0.0;
}

double CandidateMaximumRisk(
    const std::vector<eco_restoration::InvasiveControlCandidate>& candidates) {
    double maximum = 0.0;
    for (const eco_restoration::InvasiveControlCandidate& candidate : candidates) {
        maximum = std::max(maximum, candidate.risk_of_harm);
    }
    return maximum;
}

std::vector<eco_restoration::IdentifiedInvasiveControlCandidate>
IdentifyCandidates(
    const std::vector<eco_restoration::InvasiveControlCandidate>& candidates) {
    std::vector<eco_restoration::IdentifiedInvasiveControlCandidate> identified;
    identified.reserve(candidates.size());

    for (std::size_t index = 0U; index < candidates.size(); ++index) {
        identified.push_back(eco_restoration::IdentifiedInvasiveControlCandidate{
            "candidate_" + std::to_string(index + 1U),
            candidates[index]});
    }

    return identified;
}

double ComputeKnowledgeFactor(
    bool private_heat_accepted,
    bool water_valid,
    bool authorization_valid,
    bool population_valid,
    bool irrigation_valid) {
    const std::size_t satisfied =
        static_cast<std::size_t>(private_heat_accepted) +
        static_cast<std::size_t>(water_valid) +
        static_cast<std::size_t>(authorization_valid) +
        static_cast<std::size_t>(population_valid) +
        static_cast<std::size_t>(irrigation_valid);
    return static_cast<double>(satisfied) / 5.0;
}

double ComputeEcoImpactValue(
    bool water_allowed,
    bool invasive_safe,
    bool irrigation_feasible,
    bool biodiversity_compliant) {
    const std::size_t satisfied =
        static_cast<std::size_t>(water_allowed) +
        static_cast<std::size_t>(invasive_safe) +
        static_cast<std::size_t>(irrigation_feasible) +
        static_cast<std::size_t>(biodiversity_compliant);
    return static_cast<double>(satisfied) / 4.0;
}

}  // namespace

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
    double eco_impact_value) {
    FoundationReportBuilder builder;
    builder.SetPrivateHeat(private_heat_accepted)
        .SetContainmentBlocked(threat_fail_closed)
        .SetWaterAllowed(water_allowed)
        .SetWaterInvariant(water_invariant)
        .SetAuthorizationAccepted(authorization_accepted)
        .SetInvasiveSafe(invasive_safe)
        .SetIrrigationFeasible(irrigation_feasible)
        .SetScores(
            ClampUnitInterval(maximum_risk),
            ClampUnitInterval(knowledge_factor),
            ClampUnitInterval(eco_impact_value));
    return builder.Build();
}

FoundationOrchestrationResult RunFoundationSelfCheck(
    const FoundationOrchestrationInputs& inputs) {
    FoundationOrchestrationResult result;

    const bool private_heat_valid = IsValidPrivateHeat(inputs.private_heat);
    const bool threat_valid = IsValidThreat(inputs.threat);
    const bool biodiversity_valid = IsValidBiodiversity(inputs.biodiversity);
    const bool authorization_valid = IsValidAuthorization(inputs.authorization);
    const bool population_valid = IsValidPopulationModel(inputs.population_model);

    if (!private_heat_valid) {
        result.failure_reasons.emplace_back(
            "private heat statement has an invalid risk metric");
    }
    if (!threat_valid) {
        result.failure_reasons.emplace_back(
            "threat assessment has an invalid risk metric");
    }
    if (!biodiversity_valid) {
        result.failure_reasons.emplace_back(
            "biodiversity index or threshold is invalid");
    }
    if (!authorization_valid) {
        result.failure_reasons.emplace_back(
            "authorization evidence has an invalid risk metric");
    }
    if (!population_valid) {
        result.failure_reasons.emplace_back(
            "population model has invalid abundance or risk metrics");
    }

    bool invasive_inputs_valid = !inputs.invasive_candidates.empty();
    if (inputs.invasive_candidates.empty()) {
        result.failure_reasons.emplace_back(
            "at least one invasive control candidate is required");
    }
    for (const eco_restoration::InvasiveControlCandidate& candidate :
         inputs.invasive_candidates) {
        if (!IsValidInvasiveCandidate(candidate)) {
            invasive_inputs_valid = false;
            result.failure_reasons.emplace_back(
                "invasive control candidate has invalid numeric fields");
            break;
        }
    }

    const eco_restoration::WaterBiodiversityPolicyVerification water =
        eco_restoration::VerifyCrossShardWaterBiodiversityPolicy(
            inputs.water.available_water_ml,
            inputs.water.ecological_reserve_ml,
            inputs.water.allocated_water_ml,
            inputs.biodiversity.value,
            inputs.biodiversity.minimum_required,
            inputs.water.required_cross_shard_unsat);
    AppendReasons(result.failure_reasons, water.reasons, "water: ");

    const simulation::TerminalMoistureSetAnalysis irrigation =
        simulation::AnalyzeTerminalMoistureSet(
            inputs.irrigation_dynamics,
            inputs.irrigation_schedule,
            inputs.rainfall_scenarios,
            0.000001);
    AppendReasons(result.failure_reasons, irrigation.reasons, "irrigation: ");

    const std::vector<eco_restoration::IdentifiedInvasiveControlCandidate>
        identified_candidates = IdentifyCandidates(inputs.invasive_candidates);
    const eco_restoration::IdentifiedInvasiveControlSelection invasive =
        eco_restoration::SelectIdentifiedSafeInvasiveControl(
            identified_candidates);

    if (!invasive.selected) {
        result.failure_reasons.emplace_back(
            "invasive control: no safe candidate selected");
    }

    const bool authorization_accepted =
        authorization_valid &&
        inputs.authorization.verified &&
        inputs.authorization.policy_matches &&
        inputs.authorization.active &&
        inputs.authorization.risk_of_harm <= kMaximumSafeRisk;

    if (!authorization_accepted) {
        result.failure_reasons.emplace_back(
            "authorization evidence is not active, verified, policy-matched, "
            "or within the risk corridor");
    }

    const double maximum_risk = ClampUnitInterval(std::max({
        inputs.private_heat.risk_of_harm,
        inputs.threat.risk_of_harm,
        inputs.authorization.risk_of_harm,
        inputs.population_model.risk_of_harm,
        CandidateMaximumRisk(inputs.invasive_candidates)}));

    const bool irrigation_feasible =
        irrigation.valid && irrigation.all_within_bounds;
    const bool invasive_safe = invasive_inputs_valid && invasive.selected;
    const bool water_allowed = water.allowed;
    const bool water_invariant = water.invariant_holds;
    const bool private_heat_accepted =
        private_heat_valid &&
        inputs.private_heat.accepted &&
        inputs.private_heat.risk_of_harm <= kMaximumSafeRisk;
    const bool threat_fail_closed =
        threat_valid && inputs.threat.fail_closed;

    const double knowledge_factor = ComputeKnowledgeFactor(
        private_heat_valid,
        water.structurally_valid,
        authorization_valid,
        population_valid,
        irrigation.valid);
    const double eco_impact_value = ComputeEcoImpactValue(
        water_allowed,
        invasive_safe,
        irrigation_feasible,
        water.biodiversity_compliant);

    result.report = ComposeFoundationReport(
        private_heat_accepted,
        threat_fail_closed,
        water_allowed,
        water_invariant,
        authorization_accepted,
        invasive_safe,
        irrigation_feasible,
        maximum_risk,
        knowledge_factor,
        eco_impact_value);

    result.valid =
        private_heat_valid &&
        threat_valid &&
        biodiversity_valid &&
        authorization_valid &&
        population_valid &&
        invasive_inputs_valid &&
        water.structurally_valid &&
        irrigation.valid &&
        IsFoundationReportValid(result.report);

    if ((!result.valid || !result.report.foundation_safe) &&
        result.failure_reasons.empty()) {
        result.failure_reasons.emplace_back(
            "foundation report is unsafe under derived ecological constraints");
    }

    return result;
}

bool IsOrchestrationResultConsistent(
    const FoundationOrchestrationResult& result) {
    if (!IsFoundationReportValid(result.report)) {
        return false;
    }

    if (result.valid &&
        (!result.failure_reasons.empty() || !result.report.foundation_safe)) {
        return false;
    }

    if ((!result.valid || !result.report.foundation_safe) &&
        result.failure_reasons.empty()) {
        return false;
    }

    return true;
}

std::string ExplainOrchestrationFailure(
    const FoundationOrchestrationResult& result) {
    if (result.valid && result.report.foundation_safe) {
        return "foundation_orchestration=valid; foundation_safe=true";
    }

    std::ostringstream output;
    output << "foundation_orchestration=failed"
           << "; valid=" << (result.valid ? "true" : "false")
           << "; foundation_safe="
           << (result.report.foundation_safe ? "true" : "false");

    for (const std::string& reason : result.failure_reasons) {
        output << "; reason=" << reason;
    }

    return output.str();
}

bool FoundationSelfCheckOrchestratorSelfTest() {
    FoundationOrchestrationInputs safe{
        eco_restoration::PrivateHeatStatement{true, 0.10},
        eco_restoration::ThreatAssessment{true, 0.10},
        eco_restoration::WaterAllocation{2000000, 700000, 900000, false},
        eco_restoration::BiodiversityIndex{0.85, 0.70},
        eco_restoration::AuthorizationEvidence{true, true, true, 0.10},
        eco_restoration::StochasticPopulationModel{100.0, 50.0, 0.10},
        {eco_restoration::InvasiveControlCandidate{10.0, 5.0, 0.10, 50.0}},
        eco_restoration::IrrigationDynamics{0.50, 0.20, 0.90, 0.10},
        {
            eco_restoration::RainfallScenario{0.50, {0.10, 0.05}},
            eco_restoration::RainfallScenario{0.50, {0.00, 0.05}},
        },
        {0.10, 0.10}};

    const FoundationOrchestrationResult safe_result =
        RunFoundationSelfCheck(safe);
    if (!safe_result.valid ||
        !safe_result.report.foundation_safe ||
        !safe_result.failure_reasons.empty() ||
        !IsOrchestrationResultConsistent(safe_result)) {
        return false;
    }

    FoundationOrchestrationInputs unsafe_risk = safe;
    unsafe_risk.authorization.risk_of_harm = 0.31;
    const FoundationOrchestrationResult unsafe_risk_result =
        RunFoundationSelfCheck(unsafe_risk);
    if (!unsafe_risk_result.valid ||
        unsafe_risk_result.report.foundation_safe ||
        unsafe_risk_result.failure_reasons.empty() ||
        !IsOrchestrationResultConsistent(unsafe_risk_result)) {
        return false;
    }

    FoundationOrchestrationInputs missing_invariant = safe;
    missing_invariant.water.required_cross_shard_unsat = true;
    const FoundationOrchestrationResult invariant_result =
        RunFoundationSelfCheck(missing_invariant);
    if (!invariant_result.valid ||
        invariant_result.report.foundation_safe ||
        invariant_result.failure_reasons.empty()) {
        return false;
    }

    FoundationOrchestrationInputs invalid_input = safe;
    invalid_input.population_model.risk_of_harm =
        std::numeric_limits<double>::quiet_NaN();
    const FoundationOrchestrationResult invalid_result =
        RunFoundationSelfCheck(invalid_input);
    if (invalid_result.valid ||
        invalid_result.failure_reasons.empty() ||
        !IsOrchestrationResultConsistent(invalid_result)) {
        return false;
    }

    FoundationOrchestrationResult inconsistent = safe_result;
    inconsistent.failure_reasons.push_back("unexpected failure");
    if (IsOrchestrationResultConsistent(inconsistent)) {
        return false;
    }

    return ExplainOrchestrationFailure(unsafe_risk_result).find(
               "foundation_orchestration=failed") == 0U;
}

}  // namespace prometheus_praxis::foundation::orchestration
