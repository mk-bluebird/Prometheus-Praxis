// File: cpp/eco_restoration/monotone_migration_and_private_score.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eco_restoration {

/*
A half-space corridor rule is a*x+b*y<=c. For unchanged normal (a,b), a
migration is an expansion exactly when c_mutant>=c_baseline. A RoH ceiling is
relaxed exactly when roh_mutant>=roh_baseline, because more candidate actions
remain admissible.

Shrink counterexamples are:
- c_mutant<c_baseline for any nonzero normal, excluding formerly valid points;
- roh_mutant<roh_baseline, excluding a previously admissible risk interval;
- a changed normal without a separately proved set-inclusion certificate.

The property requires every generated expansion to retain all baseline-admissible
points and every generated relaxation to retain all baseline-admissible RoH
values.
*/
struct HalfSpace {
    double a{};
    double b{};
    double c{};
};

struct CorridorPolicy {
    std::vector<HalfSpace> half_spaces;
    double risk_of_harm_ceiling{};
};

struct MigrationCase {
    CorridorPolicy baseline;
    CorridorPolicy mutant;
};

inline bool point_in_corridor(double x, double y, const CorridorPolicy& policy,
                              double tolerance = 1e-9) {
    for (const auto& constraint : policy.half_spaces) {
        if (constraint.a * x + constraint.b * y > constraint.c + tolerance) {
            return false;
        }
    }
    return true;
}

inline bool risk_is_admissible(double risk_of_harm, const CorridorPolicy& policy,
                               double tolerance = 1e-9) {
    return risk_of_harm >= -tolerance &&
           risk_of_harm <= policy.risk_of_harm_ceiling + tolerance;
}

inline bool same_normals(const CorridorPolicy& baseline,
                         const CorridorPolicy& mutant,
                         double tolerance = 1e-12) {
    if (baseline.half_spaces.size() != mutant.half_spaces.size()) return false;
    for (std::size_t i = 0; i < baseline.half_spaces.size(); ++i) {
        if (std::abs(baseline.half_spaces[i].a - mutant.half_spaces[i].a) > tolerance ||
            std::abs(baseline.half_spaces[i].b - mutant.half_spaces[i].b) > tolerance) {
            return false;
        }
    }
    return true;
}

inline bool is_relaxing_migration(const MigrationCase& migration,
                                  double tolerance = 1e-12) {
    if (!same_normals(migration.baseline, migration.mutant, tolerance) ||
        migration.mutant.risk_of_harm_ceiling + tolerance <
            migration.baseline.risk_of_harm_ceiling) {
        return false;
    }
    for (std::size_t i = 0; i < migration.baseline.half_spaces.size(); ++i) {
        if (migration.mutant.half_spaces[i].c + tolerance <
            migration.baseline.half_spaces[i].c) {
            return false;
        }
    }
    return true;
}

inline MigrationCase generate_half_space_expansion(
    const CorridorPolicy& baseline, std::mt19937_64& generator,
    double maximum_offset, double maximum_risk_relaxation) {
    if (baseline.half_spaces.empty() || maximum_offset < 0.0 ||
        maximum_risk_relaxation < 0.0 ||
        baseline.risk_of_harm_ceiling < 0.0 || baseline.risk_of_harm_ceiling > 1.0) {
        throw std::invalid_argument("invalid baseline corridor policy");
    }

    std::uniform_real_distribution<double> offset(0.0, maximum_offset);
    std::uniform_real_distribution<double> risk_offset(0.0, maximum_risk_relaxation);
    CorridorPolicy mutant = baseline;

    for (auto& constraint : mutant.half_spaces) constraint.c += offset(generator);
    mutant.risk_of_harm_ceiling = std::min(
        1.0, baseline.risk_of_harm_ceiling + risk_offset(generator));
    return {baseline, std::move(mutant)};
}

inline MigrationCase shrink_half_space_counterexample(
    const CorridorPolicy& baseline, double constraint_shrink,
    double risk_tightening) {
    if (baseline.half_spaces.empty() || constraint_shrink <= 0.0 ||
        risk_tightening <= 0.0) {
        throw std::invalid_argument("positive shrink magnitudes are required");
    }

    CorridorPolicy mutant = baseline;
    mutant.half_spaces.front().c -= constraint_shrink;
    mutant.risk_of_harm_ceiling = std::max(
        0.0, baseline.risk_of_harm_ceiling - risk_tightening);
    return {baseline, std::move(mutant)};
}

inline bool preserve_baseline_samples(const MigrationCase& migration,
                                      const std::vector<std::pair<double, double>>& points,
                                      const std::vector<double>& risk_values) {
    for (const auto& [x, y] : points) {
        if (point_in_corridor(x, y, migration.baseline) &&
            !point_in_corridor(x, y, migration.mutant)) {
            return false;
        }
    }
    for (double risk : risk_values) {
        if (risk_is_admissible(risk, migration.baseline) &&
            !risk_is_admissible(risk, migration.mutant)) {
            return false;
        }
    }
    return true;
}

/*
Private eco-score guest relation:
R(w,Cw,s_min,s_max) holds when:
1. the external proof receipt binds witness commitment Cw;
2. score(w)=s and s_min<=s<=s_max;
3. RoH(w)<=0.30;
4. all quantized telemetry values satisfy declared unit and range corridors.

Public journal fields:
- schema version and guest-method identifier;
- witness-commitment reference Cw;
- bounded score interval and resulting score;
- RoH ceiling and resulting RoH;
- policy identifier, telemetry coverage, knowledge factor, eco-impact value;
- pass/fail result.

Private witness fields:
- raw and quantized telemetry records;
- sensor locations, timestamps, operator identifiers;
- intermediate score components and calibration residuals.
*/
struct PrivateEcoWitness {
    std::vector<std::int64_t> quantized_telemetry;
    double score{};
    double risk_of_harm{};
    double telemetry_coverage{};
};

struct PublicEcoScoreJournal {
    std::string schema_version;
    std::string method_identifier;
    std::string witness_commitment_reference;
    std::string policy_identifier;
    double score{};
    double score_minimum{};
    double score_maximum{};
    double risk_of_harm{};
    double risk_ceiling{};
    double telemetry_coverage{};
    double knowledge_factor{};
    double eco_impact_value{};
    bool accepted{};
};

inline PublicEcoScoreJournal build_private_score_journal(
    const PrivateEcoWitness& witness, std::string witness_commitment_reference,
    std::string policy_identifier, double score_minimum, double score_maximum,
    double minimum_telemetry_coverage) {

    if (witness.quantized_telemetry.empty() || witness_commitment_reference.empty() ||
        policy_identifier.empty() || !(score_minimum <= score_maximum) ||
        minimum_telemetry_coverage < 0.0 || minimum_telemetry_coverage > 1.0 ||
        witness.telemetry_coverage < 0.0 || witness.telemetry_coverage > 1.0 ||
        witness.risk_of_harm < 0.0 || witness.risk_of_harm > 1.0) {
        throw std::invalid_argument("invalid private eco-score relation inputs");
    }

    const bool score_in_range = witness.score >= score_minimum &&
                                witness.score <= score_maximum;
    const bool risk_safe = witness.risk_of_harm <= 0.30;
    const bool coverage_sufficient =
        witness.telemetry_coverage >= minimum_telemetry_coverage;
    const bool accepted = score_in_range && risk_safe && coverage_sufficient;

    const double score_margin = std::min(
        witness.score - score_minimum, score_maximum - witness.score);
    const double normalized_margin = score_maximum > score_minimum
        ? std::clamp(score_margin / (score_maximum - score_minimum), 0.0, 1.0)
        : 1.0;
    const double knowledge = std::clamp(
        witness.telemetry_coverage * (0.65 + 0.35 * normalized_margin), 0.0, 1.0);
    const double impact = accepted
        ? std::clamp(0.50 * knowledge + 0.50 * (1.0 - witness.risk_of_harm), 0.0, 1.0)
        : 0.0;

    return {"private_eco_score_v1", "external_guest_receipt",
            std::move(witness_commitment_reference), std::move(policy_identifier),
            witness.score, score_minimum, score_maximum, witness.risk_of_harm,
            0.30, witness.telemetry_coverage, knowledge, impact, accepted};
}

}  // namespace eco_restoration
