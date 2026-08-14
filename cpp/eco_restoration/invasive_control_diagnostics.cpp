// File: cpp/eco_restoration/invasive_control_diagnostics.cpp
#include "invasive_control_diagnostics.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace prometheus_praxis::eco_restoration {
namespace {

constexpr double kMaximumRiskOfHarm = 0.30;

bool IsLowerSnakeCase(std::string_view identifier) noexcept {
    if (identifier.empty() || identifier.front() == '_' ||
        identifier.back() == '_') {
        return false;
    }

    bool previous_underscore = false;
    for (const unsigned char character : identifier) {
        const bool lowercase = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        const bool underscore = character == '_';

        if (!lowercase && !digit && !underscore) {
            return false;
        }
        if (underscore && previous_underscore) {
            return false;
        }
        previous_underscore = underscore;
    }

    return true;
}

bool IsFiniteNonNegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool IsRiskWellFormed(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool IsStructurallyValidCandidate(
    const IdentifiedInvasiveControlCandidate& identified_candidate) noexcept {
    const InvasiveControlCandidate& candidate = identified_candidate.candidate;
    return IsLowerSnakeCase(identified_candidate.id) &&
           IsFiniteNonNegative(candidate.expected_benefit) &&
           IsFiniteNonNegative(candidate.treatment_cost) &&
           IsRiskWellFormed(candidate.risk_of_harm) &&
           IsFiniteNonNegative(candidate.expected_next_abundance);
}

std::string FormatDouble(double value) {
    if (std::isnan(value)) {
        return "nan";
    }
    if (std::isinf(value)) {
        return value > 0.0 ? "infinity" : "-infinity";
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(6) << value;
    return output.str();
}

std::string BuildCandidateSummary(
    const InvasiveTreatmentCostBenefitAudit& audit) {
    std::ostringstream output;
    output << "candidate=" << audit.candidate_id
           << "; structurally_valid="
           << (audit.structurally_valid ? "true" : "false")
           << "; benefit_covers_cost="
           << (audit.benefit_covers_cost ? "true" : "false")
           << "; risk_within_corridor="
           << (audit.risk_within_corridor ? "true" : "false")
           << "; next_state_nonnegative="
           << (audit.next_state_nonnegative ? "true" : "false")
           << "; benefit_cost_ratio="
           << FormatDouble(audit.benefit_cost_ratio)
           << "; risk_margin=" << FormatDouble(audit.risk_margin)
           << "; expected_next_abundance="
           << FormatDouble(audit.expected_next_abundance)
           << "; safe=" << (audit.safe ? "true" : "false");

    for (const std::string& reason : audit.reasons) {
        output << "; reason=" << reason;
    }

    return output.str();
}

bool IsBetterSelection(
    const InvasiveTreatmentCostBenefitAudit& candidate,
    const InvasiveTreatmentCostBenefitAudit& incumbent) noexcept {
    if (candidate.benefit_cost_ratio != incumbent.benefit_cost_ratio) {
        return candidate.benefit_cost_ratio > incumbent.benefit_cost_ratio;
    }
    if (candidate.risk_margin != incumbent.risk_margin) {
        return candidate.risk_margin > incumbent.risk_margin;
    }
    if (candidate.expected_next_abundance != incumbent.expected_next_abundance) {
        return candidate.expected_next_abundance < incumbent.expected_next_abundance;
    }
    return candidate.candidate_id < incumbent.candidate_id;
}

bool HasUniqueCandidateIds(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates) {
    std::set<std::string> identifiers;

    for (const IdentifiedInvasiveControlCandidate& candidate : candidates) {
        if (!IsLowerSnakeCase(candidate.id) ||
            !identifiers.insert(candidate.id).second) {
            return false;
        }
    }

    return true;
}

}  // namespace

double InvasiveTreatmentBenefitCostRatio(
    const InvasiveControlCandidate& candidate) {
    if (!IsFiniteNonNegative(candidate.expected_benefit) ||
        !IsFiniteNonNegative(candidate.treatment_cost)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (candidate.treatment_cost == 0.0) {
        return candidate.expected_benefit == 0.0
            ? 0.0
            : std::numeric_limits<double>::infinity();
    }

    return candidate.expected_benefit / candidate.treatment_cost;
}

InvasiveTreatmentCostBenefitAudit AuditInvasiveTreatmentCostBenefit(
    const IdentifiedInvasiveControlCandidate& identified_candidate) {
    const InvasiveControlCandidate& candidate = identified_candidate.candidate;

    InvasiveTreatmentCostBenefitAudit audit;
    audit.candidate_id = identified_candidate.id;
    audit.benefit_cost_ratio = InvasiveTreatmentBenefitCostRatio(candidate);
    audit.risk_margin = kMaximumRiskOfHarm - candidate.risk_of_harm;
    audit.expected_next_abundance = candidate.expected_next_abundance;
    audit.structurally_valid = IsStructurallyValidCandidate(identified_candidate);

    if (!IsLowerSnakeCase(identified_candidate.id)) {
        audit.reasons.emplace_back(
            "candidate id must be non-empty lower_snake_case");
    }
    if (!IsFiniteNonNegative(candidate.expected_benefit)) {
        audit.reasons.emplace_back(
            "expected benefit must be finite and non-negative");
    }
    if (!IsFiniteNonNegative(candidate.treatment_cost)) {
        audit.reasons.emplace_back(
            "treatment cost must be finite and non-negative");
    }
    if (!IsRiskWellFormed(candidate.risk_of_harm)) {
        audit.reasons.emplace_back(
            "risk of harm must be finite and within [0,1]");
    }
    if (!IsFiniteNonNegative(candidate.expected_next_abundance)) {
        audit.reasons.emplace_back(
            "expected next abundance must be finite and non-negative");
    }

    audit.benefit_covers_cost =
        IsFiniteNonNegative(candidate.expected_benefit) &&
        IsFiniteNonNegative(candidate.treatment_cost) &&
        candidate.expected_benefit >= candidate.treatment_cost;
    if (!audit.benefit_covers_cost) {
        audit.reasons.emplace_back(
            "expected benefit does not cover treatment cost");
    }

    audit.risk_within_corridor =
        IsRiskWellFormed(candidate.risk_of_harm) &&
        candidate.risk_of_harm <= kMaximumRiskOfHarm;
    if (!audit.risk_within_corridor) {
        audit.reasons.emplace_back(
            "risk of harm exceeds the 0.30 ecological safety corridor");
    }

    audit.next_state_nonnegative =
        IsFiniteNonNegative(candidate.expected_next_abundance);
    if (!audit.next_state_nonnegative) {
        audit.reasons.emplace_back(
            "projected next abundance is negative or non-finite");
    }

    audit.safe =
        audit.structurally_valid &&
        audit.benefit_covers_cost &&
        audit.risk_within_corridor &&
        audit.next_state_nonnegative;
    return audit;
}

IdentifiedInvasiveControlSelection SelectIdentifiedSafeInvasiveControl(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates) {
    IdentifiedInvasiveControlSelection selection;
    selection.candidate_summaries.reserve(candidates.size());

    if (candidates.empty()) {
        selection.explanation =
            "invasive_control_selection=not_selected; reason=no_candidates";
        return selection;
    }

    const bool identifiers_valid = HasUniqueCandidateIds(candidates);
    std::vector<InvasiveTreatmentCostBenefitAudit> audits;
    audits.reserve(candidates.size());

    for (const IdentifiedInvasiveControlCandidate& candidate : candidates) {
        InvasiveTreatmentCostBenefitAudit audit =
            AuditInvasiveTreatmentCostBenefit(candidate);

        if (!identifiers_valid) {
            audit.safe = false;
            audit.reasons.emplace_back(
                "selection requires unique lower_snake_case candidate ids");
        }

        selection.candidate_summaries.push_back(BuildCandidateSummary(audit));
        audits.push_back(std::move(audit));
    }

    if (!identifiers_valid) {
        selection.explanation =
            "invasive_control_selection=not_selected; "
            "reason=invalid_or_duplicate_candidate_id";
        return selection;
    }

    const InvasiveTreatmentCostBenefitAudit* best = nullptr;
    for (const InvasiveTreatmentCostBenefitAudit& audit : audits) {
        if (audit.safe && (best == nullptr || IsBetterSelection(audit, *best))) {
            best = &audit;
        }
    }

    if (best == nullptr) {
        selection.explanation =
            "invasive_control_selection=not_selected; reason=no_safe_candidate";
        return selection;
    }

    selection.selected = true;
    selection.selected_id = best->candidate_id;
    selection.explanation =
        "invasive_control_selection=selected; selected_id=" +
        selection.selected_id +
        "; benefit_cost_ratio=" + FormatDouble(best->benefit_cost_ratio) +
        "; risk_margin=" + FormatDouble(best->risk_margin) +
        "; expected_next_abundance=" +
        FormatDouble(best->expected_next_abundance);
    return selection;
}

std::string ExplainStochasticHjbSelection(
    const std::vector<IdentifiedInvasiveControlCandidate>& candidates,
    const IdentifiedInvasiveControlSelection& selection) {
    std::ostringstream output;
    output << "invasive_control_selection_audit"
           << "; candidate_count=" << candidates.size()
           << "; selected=" << (selection.selected ? "true" : "false")
           << "; selected_id="
           << (selection.selected ? selection.selected_id : "none")
           << "; explanation=" << selection.explanation;

    for (const std::string& summary : selection.candidate_summaries) {
        output << "; " << summary;
    }

    return output.str();
}

bool InvasiveControlDiagnosticsSelfTest() {
    const std::vector<IdentifiedInvasiveControlCandidate> valid_candidates{
        {"manual_removal", {10.0, 5.0, 0.10, 30.0}},
        {"targeted_cutting", {12.0, 6.0, 0.10, 20.0}},
        {"native_competition", {9.0, 6.0, 0.20, 15.0}}};

    const IdentifiedInvasiveControlSelection valid_selection =
        SelectIdentifiedSafeInvasiveControl(valid_candidates);
    if (!valid_selection.selected ||
        valid_selection.selected_id != "targeted_cutting") {
        return false;
    }

    const std::vector<IdentifiedInvasiveControlCandidate> no_safe{
        {"high_risk_method", {20.0, 5.0, 0.31, 2.0}},
        {"uneconomical_method", {2.0, 5.0, 0.10, 2.0}}};
    if (SelectIdentifiedSafeInvasiveControl(no_safe).selected) {
        return false;
    }

    const std::vector<IdentifiedInvasiveControlCandidate> duplicate{
        {"manual_removal", {10.0, 5.0, 0.10, 20.0}},
        {"manual_removal", {12.0, 6.0, 0.10, 18.0}}};
    if (SelectIdentifiedSafeInvasiveControl(duplicate).selected) {
        return false;
    }

    const IdentifiedInvasiveControlCandidate invalid_numeric{
        "invalid_numeric",
        {std::numeric_limits<double>::quiet_NaN(), 2.0, 0.10, 4.0}};
    const InvasiveTreatmentCostBenefitAudit invalid_audit =
        AuditInvasiveTreatmentCostBenefit(invalid_numeric);
    if (invalid_audit.structurally_valid || invalid_audit.safe) {
        return false;
    }

    const InvasiveControlCandidate zero_cost{1.0, 0.0, 0.10, 1.0};
    if (!std::isinf(InvasiveTreatmentBenefitCostRatio(zero_cost))) {
        return false;
    }

    const std::vector<IdentifiedInvasiveControlCandidate> tied{
        {"zeta_method", {10.0, 5.0, 0.10, 10.0}},
        {"alpha_method", {10.0, 5.0, 0.10, 10.0}}};
    const IdentifiedInvasiveControlSelection tie_selection =
        SelectIdentifiedSafeInvasiveControl(tied);
    if (!tie_selection.selected ||
        tie_selection.selected_id != "alpha_method") {
        return false;
    }

    const IdentifiedInvasiveControlSelection empty_selection =
        SelectIdentifiedSafeInvasiveControl({});
    if (empty_selection.selected ||
        empty_selection.explanation.find("reason=no_candidates") ==
            std::string::npos) {
        return false;
    }

    const std::string explanation =
        ExplainStochasticHjbSelection(valid_candidates, valid_selection);
    return explanation.find("selected_id=targeted_cutting") !=
           std::string::npos;
}

}  // namespace prometheus_praxis::eco_restoration
