// File: cpp/eco_restoration/private_heat_membership_threat_model.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

/*
Private membership-and-heat relation:

exists c:
  external_commitment_binds(c) and
  lookup(c.h3_index, corridor_table) and
  c.heat_index + uncertainty_margin < heat_critical.

Public statement:
- corridor-table identifier and declared H3 resolution;
- heat-critical threshold, uncertainty margin, and fixed-point scale;
- proof-system identifier and accepted Boolean result.

Private witness:
- H3 cell index, heat index, underlying observation provenance, and
  quantized sensor or remote-sensing values.

Lookup plan:
- Corridor membership table: exactly |R| H3-index rows.
- Heat range table: 2^16=65,536 entries per unsigned 16-bit limb.
- A signed 32-bit heat value uses two 16-bit range lookups.
- Engineering proof-artifact target: at most 16 KiB public proof plus journal.
The target is an integration budget, not a universal proof-system guarantee.
*/
struct PrivateHeatProofPlan {
    std::size_t corridor_cell_count{};
    std::uint8_t h3_resolution{};
    std::int64_t fixed_point_scale{};
    std::int64_t heat_critical_fixed{};
    std::int64_t uncertainty_margin_fixed{};
    std::size_t proof_target_bytes{16U * 1024U};
};

struct PrivateHeatStatement {
    std::string corridor_table_identifier;
    std::string proof_system_identifier;
    bool accepted{};
    std::size_t membership_lookup_rows{};
    std::size_t heat_range_lookup_rows{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline PrivateHeatStatement build_private_heat_statement(
    const PrivateHeatProofPlan& plan, bool externally_proven_membership,
    bool externally_proven_heat_bound, double data_completeness) {

    if (plan.corridor_cell_count == 0 || plan.h3_resolution > 15 ||
        plan.fixed_point_scale <= 0 || plan.uncertainty_margin_fixed < 0 ||
        !(data_completeness >= 0.0 && data_completeness <= 1.0)) {
        throw std::invalid_argument("invalid private heat-proof plan");
    }

    const bool accepted = externally_proven_membership &&
                          externally_proven_heat_bound;
    const std::size_t heat_lookup_rows = std::size_t{1} << 16;
    const double margin_ratio = static_cast<double>(plan.uncertainty_margin_fixed) /
        std::max(1.0, std::abs(static_cast<double>(plan.heat_critical_fixed)));
    const double knowledge = accepted
        ? std::clamp(data_completeness * (1.0 - std::min(0.45, margin_ratio)), 0.0, 1.0)
        : 0.0;
    const double impact = accepted
        ? std::clamp(0.60 * knowledge + 0.40, 0.0, 1.0)
        : 0.0;

    return {"corridor_h3_table", "external_private_proof", accepted,
            plan.corridor_cell_count, heat_lookup_rows, knowledge, impact};
}

enum class ThreatSurface {
    SensorSpoofing,
    ModelPoisoning,
    PolicySubstitution,
    DelayedActuation
};

struct ThreatObservation {
    ThreatSurface surface{};
    double anomaly_score{};
    double provenance_coverage{};
    double policy_match_score{};
    double actuation_delay_ratio{};
};

struct ThreatAssessment {
    double detectability{};
    double estimated_risk_of_harm{};
    bool unsafe_condition{};
    bool fail_closed{};
    double knowledge_factor{};
    double eco_impact_value{};
};

/*
Defensive adversarial game:
the evaluator seeks the smallest detectable perturbation that could make
estimated RoH exceed 0.30. The defender combines independent anomaly,
provenance, policy-consistency, and timing signals. Any unsafe estimate or
insufficient evidence produces fail_closed=true.

This is a detection-and-containment model only. It does not generate sensor,
model, policy, or actuation manipulations.
*/
inline ThreatAssessment assess_ecological_system_threat(
    const std::vector<ThreatObservation>& observations,
    double baseline_risk_of_harm) {

    if (observations.empty() || baseline_risk_of_harm < 0.0 ||
        baseline_risk_of_harm > 1.0) {
        throw std::invalid_argument("invalid threat-model inputs");
    }

    double maximum_anomaly = 0.0;
    double minimum_provenance = 1.0;
    double minimum_policy_match = 1.0;
    double maximum_delay = 0.0;

    for (const auto& observation : observations) {
        if (observation.anomaly_score < 0.0 || observation.anomaly_score > 1.0 ||
            observation.provenance_coverage < 0.0 || observation.provenance_coverage > 1.0 ||
            observation.policy_match_score < 0.0 || observation.policy_match_score > 1.0 ||
            observation.actuation_delay_ratio < 0.0 || observation.actuation_delay_ratio > 1.0) {
            throw std::invalid_argument("threat observation must lie in [0,1]");
        }
        maximum_anomaly = std::max(maximum_anomaly, observation.anomaly_score);
        minimum_provenance = std::min(minimum_provenance, observation.provenance_coverage);
        minimum_policy_match = std::min(minimum_policy_match, observation.policy_match_score);
        maximum_delay = std::max(maximum_delay, observation.actuation_delay_ratio);
    }

    const double detectability = std::clamp(
        0.40 * maximum_anomaly +
        0.25 * (1.0 - minimum_provenance) +
        0.20 * (1.0 - minimum_policy_match) +
        0.15 * maximum_delay, 0.0, 1.0);

    const double estimated_risk = std::clamp(
        baseline_risk_of_harm +
        0.35 * maximum_anomaly +
        0.20 * (1.0 - minimum_provenance) +
        0.20 * (1.0 - minimum_policy_match) +
        0.25 * maximum_delay, 0.0, 1.0);

    const bool unsafe = estimated_risk > 0.30;
    const bool fail_closed = unsafe || minimum_provenance < 0.90 ||
                             minimum_policy_match < 0.98;
    const double knowledge = std::clamp(
        minimum_provenance * minimum_policy_match * (1.0 - maximum_anomaly), 0.0, 1.0);
    const double impact = fail_closed ? 0.0 :
        std::clamp(knowledge * (1.0 - estimated_risk), 0.0, 1.0);

    return {detectability, estimated_risk, unsafe, fail_closed, knowledge, impact};
}

}  // namespace eco_restoration
