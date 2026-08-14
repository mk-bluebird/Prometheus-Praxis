// File: cpp/eco_restoration/invasive_control_diagnostics.cpp
#include "invasive_control_diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr double kMaximumRiskOfHarm = 0.30;

bool IsFiniteNonNegative(const double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool IsLowerSnakeCase(const std::string& id) noexcept {
    if (id.empty() || id.front() == '_' || id.back() == '_') {
        return false;
    }

    bool previous_was_underscore = false;
    for (const char character : id) {
        const bool lower_case = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';

        if (character == '_') {
            if (previous_was_underscore) {
                return false;
            }
            previous_was_underscore = true;
            continue;
        }

        if (!lower_case && !digit) {
            return false;
        }
        previous_was_underscore = false;
    }

    return true;
}

bool HasUniqueValidIds(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates) {
    for (std::size_t left = 0U; left < candidates.size(); ++left) {
        if (!IsLowerSnakeCase(candidates[left].id)) {
            return false;
        }

        for (std::size_t right = left + 1U; right < candidates.size();
             ++right) {
            if (candidates[left].id == candidates[right].id) {
                return false;
            }
        }
    }
    return true;
}

bool CandidateIsStructurallyValid(
    const eco_restoration::InvasiveControlCandidate& candidate) noexcept {
    return IsFiniteNonNegative(candidate.expected_benefit) &&
           IsFiniteNonNegative(candidate.treatment_cost) &&
           IsFiniteNonNegative(candidate.risk_of_harm) &&
           IsFiniteNonNegative(candidate.expected_next_abundance);
}

bool IsSaferCandidate(
    const InvasiveTreatmentCostBenefitAudit& candidate,
    const InvasiveTreatmentCostBenefitAudit& current_best) noexcept {
    if (candidate.benefit_cost_ratio != current_best.benefit_cost_ratio) {
        return candidate.benefit_cost_ratio > current_best.benefit_cost_ratio;
    }
    if (candidate.risk_of_harm != current_best.risk_of_harm) {
        return candidate.risk_of_harm < current_best.risk_of_harm;
    }
    return candidate.candidate_id < current_best.candidate_id;
}

}  // namespace

double InvasiveTreatmentBenefitCostRatio(
    const eco_restoration::InvasiveControlCandidate& candidate) {
    if (!CandidateIsStructurallyValid(candidate)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (candidate.treatment_cost == 0.0) {
        return candidate.expected_benefit > 0.0
                   ? std::numeric_limits<double>::infinity()
                   : 1.0;
    }

    return candidate.expected_benefit / candidate.treatment_cost;
}

InvasiveTreatmentCostBenefitAudit AuditInvasiveTreatmentCostBenefit(
    const IdentifiedInvasiveControlCandidate& identified_candidate) {
    const eco_restoration::InvasiveControlCandidate& candidate =
        identified_candidate.candidate;

    InvasiveTreatmentCostBenefitAudit audit{};
    audit.candidate_id = identified_candidate.id;
    audit.expected_next_abundance = candidate.expected_next_abundance;

    if (!IsLowerSnakeCase(identified_candidate.id)) {
        audit.reasons.emplace_back(
            "candidate id must be non-empty lower_snake_case");
    }

    if (!CandidateIsStructurallyValid(candidate)) {
        audit.reasons.emplace_back(
            "candidate benefit, cost, risk, and next abundance must be finite "
            "non-negative values");
    }

    audit.structurally_valid = audit.reasons.empty();
    if (!audit.structurally_valid) {
        audit.benefit_cost_ratio =
            std::numeric_limits<double>::quiet_NaN();
        audit.risk_margin = std::numeric_limits<double>::quiet_NaN();
        audit.safe = false;
        return audit;
    }

    audit.benefit_cost_ratio = InvasiveTreatmentBenefitCostRatio(candidate);
    audit.risk_margin = kMaximumRiskOfHarm - candidate.risk_of_harm;
    audit.benefit_covers_cost =
        candidate.expected_benefit >= candidate.treatment_cost;
    audit.risk_within_corridor =
        candidate.risk_of_harm <= kMaximumRiskOfHarm;
    audit.next_state_nonnegative = candidate.expected_next_abundance >= 0.0;

    if (!audit.benefit_covers_cost) {
        audit.reasons.emplace_back("expected benefit does not cover treatment cost");
    }
    if (!audit.risk_within_corridor) {
        audit.reasons.emplace_back("risk of harm exceeds the 0.30 corridor");
    }
    if (!audit.next_state_nonnegative) {
        audit.reasons.emplace_back("expected next abundance is negative");
    }

    audit.safe = audit.structurally_valid &&
                 audit.benefit_covers_cost &&
                 audit.risk_within_corridor &&
                 audit.next_state_nonnegative;
    return audit;
}

std::optional<IdentifiedInvasiveControlCandidate>
SelectIdentifiedSafeInvasiveControl(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates) {
    if (!HasUniqueValidIds(candidates)) {
        return std::nullopt;
    }

    std::optional<IdentifiedInvasiveControlCandidate> selected;
    std::optional<InvasiveTreatmentCostBenefitAudit> selected_audit;

    for (const IdentifiedInvasiveControlCandidate& candidate : candidates) {
        const InvasiveTreatmentCostBenefitAudit audit =
            AuditInvasiveTreatmentCostBenefit(candidate);

        if (!audit.safe) {
            continue;
        }

        if (!selected_audit.has_value() ||
            IsSaferCandidate(audit, *selected_audit)) {
            selected = candidate;
            selected_audit = audit;
        }
    }

    return selected;
}

std::string ExplainStochasticHjbSelection(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates,
    const std::optional<IdentifiedInvasiveControlCandidate>& selected) {
    std::ostringstream output;
    output << "candidate_count=" << candidates.size();

    if (!HasUniqueValidIds(candidates)) {
        output << "\nselection=none"
               << "\nreason=candidate identifiers must be unique lower_snake_case";
        return output.str();
    }

    for (const IdentifiedInvasiveControlCandidate& candidate : candidates) {
        const InvasiveTreatmentCostBenefitAudit audit =
            AuditInvasiveTreatmentCostBenefit(candidate);

        output << '\n' << "candidate=" << candidate.id
               << ", safe=" << (audit.safe ? "1" : "0")
               << ", benefit_cost_ratio=" << audit.benefit_cost_ratio
               << ", risk_margin=" << audit.risk_margin
               << ", expected_next_abundance="
               << audit.expected_next_abundance;
    }

    if (!selected.has_value()) {
        output << "\nselection=none"
               << "\nreason=no candidate satisfies benefit, risk, and next-state "
                  "safety constraints";
        return output.str();
    }

    output << "\nselection=" << selected->id
           << "\nreason=selected by stable candidate identity after safety "
              "filtering and deterministic benefit-cost ranking";
    return output.str();
}

bool InvasiveControlDiagnosticsSelfTest() {
    const std::vector<IdentifiedInvasiveControlCandidate> candidates{
        IdentifiedInvasiveControlCandidate{
            "native_seedling_release",
            eco_restoration::InvasiveControlCandidate{
                120.0, 80.0, 0.20, 35.0}},
        IdentifiedInvasiveControlCandidate{
            "high_risk_removal",
            eco_restoration::InvasiveControlCandidate{
                200.0, 100.0, 0.35, 20.0}},
        IdentifiedInvasiveControlCandidate{
            "low_benefit_treatment",
            eco_restoration::InvasiveControlCandidate{
                30.0, 50.0, 0.10, 45.0}}};

    const std::optional<IdentifiedInvasiveControlCandidate> selected =
        SelectIdentifiedSafeInvasiveControl(candidates);

    if (!selected.has_value() ||
        selected->id != "native_seedling_release") {
        return false;
    }

    const InvasiveTreatmentCostBenefitAudit audit =
        AuditInvasiveTreatmentCostBenefit(*selected);
    if (!audit.structurally_valid ||
        !audit.safe ||
        !audit.benefit_covers_cost ||
        !audit.risk_within_corridor ||
        !audit.next_state_nonnegative) {
        return false;
    }

    const std::vector<IdentifiedInvasiveControlCandidate> unsafe{
        IdentifiedInvasiveControlCandidate{
            "unsafe_candidate",
            eco_restoration::InvasiveControlCandidate{
                10.0, 20.0, 0.40, 0.0}}};

    if (SelectIdentifiedSafeInvasiveControl(unsafe).has_value()) {
        return false;
    }

    std::vector<IdentifiedInvasiveControlCandidate> duplicate_ids = candidates;
    duplicate_ids.push_back(
        IdentifiedInvasiveControlCandidate{
            "native_seedling_release",
            eco_restoration::InvasiveControlCandidate{
                90.0, 30.0, 0.10, 10.0}});

    if (SelectIdentifiedSafeInvasiveControl(duplicate_ids).has_value()) {
        return false;
    }

    const IdentifiedInvasiveControlCandidate invalid_candidate{
        "invalid_candidate",
        eco_restoration::InvasiveControlCandidate{
            -1.0, 10.0, 0.10, 10.0}};

    if (AuditInvasiveTreatmentCostBenefit(invalid_candidate).structurally_valid) {
        return false;
    }

    const eco_restoration::InvasiveControlCandidate zero_cost{
        10.0, 0.0, 0.10, 5.0};

    if (!std::isinf(InvasiveTreatmentBenefitCostRatio(zero_cost))) {
        return false;
    }

    const std::string explanation =
        ExplainStochasticHjbSelection(candidates, selected);
    return explanation.find("selection=native_seedling_release") !=
           std::string::npos;
}
