// File: cpp/eco_restoration/invasive_control_diagnostics.hpp
#pragma once

#include <string>
#include <vector>

namespace prometheus_praxis::eco_restoration {

inline constexpr double kMaximumInvasiveControlRiskOfHarm = 0.30;

struct InvasiveControlCandidate {
    double expected_benefit{};
    double treatment_cost{};
    double risk_of_harm{};
    double expected_next_abundance{};
};

struct IdentifiedInvasiveControlCandidate {
    std::string id;
    InvasiveControlCandidate candidate;
};

struct InvasiveTreatmentCostBenefitAudit {
    std::string candidate_id;
    bool structurally_valid{};
    bool benefit_covers_cost{};
    bool risk_within_corridor{};
    bool next_state_nonnegative{};
    bool safe{};
    double benefit_cost_ratio{};
    double risk_margin{};
    double expected_next_abundance{};
    std::vector<std::string> reasons;
};

struct IdentifiedInvasiveControlSelection {
    bool selected{};
    std::string selected_id;
    std::string explanation;
    std::vector<std::string> candidate_summaries;
};

double InvasiveTreatmentBenefitCostRatio(
    const InvasiveControlCandidate& candidate) noexcept;

InvasiveTreatmentCostBenefitAudit AuditInvasiveTreatmentCostBenefit(
    const IdentifiedInvasiveControlCandidate& identified_candidate);

IdentifiedInvasiveControlSelection SelectIdentifiedSafeInvasiveControl(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates);

std::string ExplainStochasticHjbSelection(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates,
    const IdentifiedInvasiveControlSelection& selection);

bool InvasiveControlDiagnosticsSelfTest();

}  // namespace prometheus_praxis::eco_restoration
