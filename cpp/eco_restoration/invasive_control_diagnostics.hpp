// File: cpp/eco_restoration/invasive_control_diagnostics.hpp
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace eco_restoration {

struct InvasiveControlCandidate {
    double expected_benefit{};
    double treatment_cost{};
    double risk_of_harm{};
    double expected_next_abundance{};
};

}  // namespace eco_restoration

struct IdentifiedInvasiveControlCandidate {
    std::string id;
    eco_restoration::InvasiveControlCandidate candidate;
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

InvasiveTreatmentCostBenefitAudit AuditInvasiveTreatmentCostBenefit(
    const IdentifiedInvasiveControlCandidate& identified_candidate);

double InvasiveTreatmentBenefitCostRatio(
    const eco_restoration::InvasiveControlCandidate& candidate);

std::optional<IdentifiedInvasiveControlCandidate>
SelectIdentifiedSafeInvasiveControl(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates);

std::string ExplainStochasticHjbSelection(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates,
    const std::optional<IdentifiedInvasiveControlCandidate>& selected);

bool InvasiveControlDiagnosticsSelfTest();
