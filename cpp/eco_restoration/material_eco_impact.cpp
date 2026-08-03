// File: cpp/eco_restoration/material_eco_impact.cpp
#include <cmath>
#include <string>
#include <algorithm>

namespace eco_restoration {

struct MaterialTestParams {
    // ISO 14851 / OECD 301-style biodegradation metrics
    double oxygen_depletion_percent;   // % O2 depletion vs reference
    double co2_evolution_percent;      // % CO2 vs reference
    double bod_removal_percent;        // % BOD removal
    double doc_removal_percent;        // % DOC removal
    double days_to_pass_window;        // days to reach pass threshold (<= 10 for ready biodegradability)

    // Additional eco-restoration parameters
    double toxicity_score;             // normalized [0,1], higher is more toxic
    double pfas_presence;              // normalized [0,1], PFAS burden
};

struct MaterialEcoImpact {
    double k_safe_fraction;            // fraction of safe operational steps in corridor
    double e_eco_benefit_band;         // normalized eco-benefit band [0,1]
    double r_risk_max;                 // max normalized risk [0,1]
    double ker_score;                  // KER composite: k * e - r
    double biodegradability_score;     // [0,1], higher means more readily biodegradable
};

// Internal helper: compute biodegradability score from standardized test metrics.
inline double compute_biodegradability_score(const MaterialTestParams& p) {
    // Based on OECD/ISO guidance: pass levels are typically:
    // - >= 60% BOD or CO2
    // - >= 70% DOC removal
    // within a 10-day window after biodegradation onset [66][72][75].
    double bod_norm = std::clamp(p.bod_removal_percent / 60.0, 0.0, 1.0);
    double co2_norm = std::clamp(p.co2_evolution_percent / 60.0, 0.0, 1.0);
    double doc_norm = std::clamp(p.doc_removal_percent / 70.0, 0.0, 1.0);

    // Days-to-pass window: 10 days or less is ideal; we penalize longer windows.
    double window_norm;
    if (p.days_to_pass_window <= 0.0) {
        window_norm = 0.0;
    } else if (p.days_to_pass_window <= 10.0) {
        window_norm = 1.0;
    } else {
        window_norm = std::clamp(10.0 / p.days_to_pass_window, 0.0, 1.0);
    }

    // Aggregate into a single biodegradability score.
    double score = 0.25 * bod_norm + 0.25 * co2_norm + 0.25 * doc_norm + 0.25 * window_norm;
    return std::clamp(score, 0.0, 1.0);
}

// Internal helper: compute K, E, R from test parameters and biodegradability.
inline MaterialEcoImpact compute_impact_internal(const MaterialTestParams& p) {
    MaterialEcoImpact impact{};

    impact.biodegradability_score = compute_biodegradability_score(p);

    // Eco-benefit band increases with biodegradability, decreases with toxicity/PFAS.
    double toxicity_penalty = std::clamp(p.toxicity_score, 0.0, 1.0);
    double pfas_penalty     = std::clamp(p.pfas_presence, 0.0, 1.0);

    double eco_raw = impact.biodegradability_score * (1.0 - 0.5 * toxicity_penalty) * (1.0 - 0.5 * pfas_penalty);
    impact.e_eco_benefit_band = std::clamp(eco_raw, 0.0, 1.0);

    // Risk max aggregates toxicity and PFAS burden.
    impact.r_risk_max = std::clamp(0.5 * toxicity_penalty + 0.5 * pfas_penalty, 0.0, 1.0);

    // K safe fraction approximates fraction of corridor steps that remain within safe bands.
    // Here we model K as a function of eco-benefit and inverse risk.
    impact.k_safe_fraction = std::clamp(
        0.5 * impact.e_eco_benefit_band + 0.5 * (1.0 - impact.r_risk_max),
        0.0,
        1.0
    );

    impact.ker_score = impact.k_safe_fraction * impact.e_eco_benefit_band - impact.r_risk_max;

    return impact;
}

} // namespace eco_restoration

extern "C" {

// Pure C API for use from Rust crates and other languages.
// No new crates are required; consumers can link to this C++ object file directly.

typedef struct {
    double oxygen_depletion_percent;
    double co2_evolution_percent;
    double bod_removal_percent;
    double doc_removal_percent;
    double days_to_pass_window;
    double toxicity_score;
    double pfas_presence;
} MaterialTestParamsC;

typedef struct {
    double k_safe_fraction;
    double e_eco_benefit_band;
    double r_risk_max;
    double ker_score;
    double biodegradability_score;
} MaterialEcoImpactC;

// Compute eco-impact scores (KER + biodegradability) from standardized test parameters.
// Returns a struct suitable for FFI into Rust and other languages.
MaterialEcoImpactC compute_material_eco_impact(MaterialTestParamsC params) {
    eco_restoration::MaterialTestParams p{};
    p.oxygen_depletion_percent = params.oxygen_depletion_percent;
    p.co2_evolution_percent    = params.co2_evolution_percent;
    p.bod_removal_percent      = params.bod_removal_percent;
    p.doc_removal_percent      = params.doc_removal_percent;
    p.days_to_pass_window      = params.days_to_pass_window;
    p.toxicity_score           = params.toxicity_score;
    p.pfas_presence            = params.pfas_presence;

    eco_restoration::MaterialEcoImpact impact =
        eco_restoration::compute_impact_internal(p);

    MaterialEcoImpactC out{};
    out.k_safe_fraction        = impact.k_safe_fraction;
    out.e_eco_benefit_band     = impact.e_eco_benefit_band;
    out.r_risk_max             = impact.r_risk_max;
    out.ker_score              = impact.ker_score;
    out.biodegradability_score = impact.biodegradability_score;

    return out;
}

} // extern "C"
