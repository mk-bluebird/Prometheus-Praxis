// File: cpp/tools/bugs_life_research_objects.cpp
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <iomanip>
#include <cmath>

namespace ppx {
namespace eco {
namespace bugs_life {

/// Seven-dimensional governance profile, aligned with Prometheus-Praxis rubric.
/// Dimensions: KnowledgeFactor, EcoImpact, RiskOfHarm, Robustness,
/// Sovereignty, EnergyEfficiency, GovernanceAlignment.
struct SevenDimProfile {
    double knowledge_factor;       // 0.0 .. 1.0
    double eco_impact;             // 0.0 .. 1.0
    double risk_of_harm;           // 0.0 .. 1.0 (lower is better)
    double robustness;             // 0.0 .. 1.0
    double sovereignty;            // 0.0 .. 1.0
    double energy_efficiency;      // 0.0 .. 1.0
    double governance_alignment;   // 0.0 .. 1.0
};

/// Evidence flags for PhoenixEligibilityGate-style governance.
struct SystemEvidenceFlags {
    bool domain_performance_ok;
    bool safety_case_documented;
    bool sovereignty_compliant;
    bool energy_neutral_or_renew;
    bool explainable_and_audited;
};

/// Corridors for individual risk coordinates in BugsLife.
/// All values normalized to [0,1].
struct BugsLifeRiskCoordinates {
    // Acoustic / sound
    double r_noise_level;      // normalized A-weighted sound level vs limits
    double r_noise_exposure;   // time-integrated exposure vs safe band
    double r_noise_annoyance;  // subjective annoyance corridor

    // Odor / chemical perception
    double r_odor_intensity;   // perceived odor strength
    double r_odor_hours;       // odor-hours vs allowed episodes
    double r_odor_tox;         // toxicity corridor penetration

    // Light / visual
    double r_light_glare;      // glare / light intrusion
    double r_laser_class;      // laser hazard index
    double r_light_flicker;    // flicker/strobe risk

    // Toxicity / biophysical
    double r_toxicity_acute;
    double r_toxicity_chronic;
    double r_bioaccumulation;

    // Stress / disturbance
    double r_disturbance_freq;
    double r_disturbance_duty;
    double r_ecosystem_sensitivity;
};

/// Simple weight bundle for aggregating risk coordinates into a Lyapunov-style residual.
struct BugsLifeRiskWeights {
    double w_noise_level;
    double w_noise_exposure;
    double w_noise_annoyance;

    double w_odor_intensity;
    double w_odor_hours;
    double w_odor_tox;

    double w_light_glare;
    double w_laser_class;
    double w_light_flicker;

    double w_toxicity_acute;
    double w_toxicity_chronic;
    double w_bioaccumulation;

    double w_disturbance_freq;
    double w_disturbance_duty;
    double w_ecosystem_sensitivity;
};

/// Core shard identifier for research objects.
struct ResearchObjectId {
    std::string category; // "component", "experiment", "safety_case", "sovereignty", "energy", "explainability"
    std::string name;
    std::string version;
};

/// Metrics for BugsLife experiments (non-lethal pest deterrent deployments).
struct ExperimentMetrics {
    double mean_pest_reduction;     // fraction reduction vs baseline (0..1, higher is better)
    double p_value_effect;          // statistical significance for deterrent efficacy
    double kg_poison_avoided_per_day;
    double non_target_incidents_per_month;
    double avg_noise_level;         // normalized 0..1 relative to corridor
    double avg_odor_intensity;      // normalized 0..1 relative to corridor
};

/// Experiment research object for corridor-governed Pest Deterrent Signal Systems.
struct ExperimentResearchObject {
    ResearchObjectId id;
    std::string corridor_id;
    ExperimentMetrics metrics;
    SystemEvidenceFlags gate_impact;
    BugsLifeRiskCoordinates risk_coords;
};

/// Snapshot of system state for AI-chat and governance tooling.
struct ResearchSnapshot {
    SevenDimProfile system_profile;
    SystemEvidenceFlags evidence_flags;
    std::vector<ExperimentResearchObject> experiments;
};

/// Clamp helper to keep values in [0,1].
inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

/// Compute Lyapunov-style residual V_t = sum_j w_j * r_j^2
/// Risk coordinates and weights are assumed non-negative.
double compute_bugs_life_residual(const BugsLifeRiskCoordinates& r,
                                  const BugsLifeRiskWeights& w) {
    double vt = 0.0;

    vt += w.w_noise_level        * r.r_noise_level        * r.r_noise_level;
    vt += w.w_noise_exposure     * r.r_noise_exposure     * r.r_noise_exposure;
    vt += w.w_noise_annoyance    * r.r_noise_annoyance    * r.r_noise_annoyance;

    vt += w.w_odor_intensity     * r.r_odor_intensity     * r.r_odor_intensity;
    vt += w.w_odor_hours         * r.r_odor_hours         * r.r_odor_hours;
    vt += w.w_odor_tox           * r.r_odor_tox           * r.r_odor_tox;

    vt += w.w_light_glare        * r.r_light_glare        * r.r_light_glare;
    vt += w.w_laser_class        * r.r_laser_class        * r.r_laser_class;
    vt += w.w_light_flicker      * r.r_light_flicker      * r.r_light_flicker;

    vt += w.w_toxicity_acute     * r.r_toxicity_acute     * r.r_toxicity_acute;
    vt += w.w_toxicity_chronic   * r.r_toxicity_chronic   * r.r_toxicity_chronic;
    vt += w.w_bioaccumulation    * r.r_bioaccumulation    * r.r_bioaccumulation;

    vt += w.w_disturbance_freq   * r.r_disturbance_freq   * r.r_disturbance_freq;
    vt += w.w_disturbance_duty   * r.r_disturbance_duty   * r.r_disturbance_duty;
    vt += w.w_ecosystem_sensitivity * r.r_ecosystem_sensitivity * r.r_ecosystem_sensitivity;

    return vt;
}

/// Hard rule: "No corridor, no deployment".
/// Returns true if all mandatory risk coordinates are strictly below 1.0.
bool corridor_complete(const BugsLifeRiskCoordinates& r) {
    const std::array<double, 16> coords = {
        r.r_noise_level,
        r.r_noise_exposure,
        r.r_noise_annoyance,
        r.r_odor_intensity,
        r.r_odor_hours,
        r.r_odor_tox,
        r.r_light_glare,
        r.r_laser_class,
        r.r_light_flicker,
        r.r_toxicity_acute,
        r.r_toxicity_chronic,
        r.r_bioaccumulation,
        r.r_disturbance_freq,
        r.r_disturbance_duty,
        r.r_ecosystem_sensitivity,
        r.r_odor_tox // toxicity corridor is considered critical
    };

    for (double c : coords) {
        if (c >= 1.0) {
            return false;
        }
    }
    return true;
}

/// Hard rule: "Violated corridor ⇒ derate/stop".
/// Returns true if V_{t+1} <= V_t, otherwise signals a breach.
bool residual_safe(double vt, double vt_next) {
    return vt_next <= vt;
}

/// Derive SevenDimProfile for a BugsLife deployment from K/E/R and residual.
/// Uses conceptual values consistent with K≈0.90, E≈0.92, R≈0.14 for research lane.
SevenDimProfile derive_profile_from_ker(double knowledge_factor,
                                        double eco_impact,
                                        double risk_of_harm_residual) {
    SevenDimProfile p{};
    p.knowledge_factor     = clamp01(knowledge_factor);
    p.eco_impact           = clamp01(eco_impact);
    p.risk_of_harm         = clamp01(risk_of_harm_residual);
    // For BugsLife, robustness grows as residual shrinks.
    p.robustness           = clamp01(1.0 - risk_of_harm_residual * 0.5);
    // Sovereignty & governance: corridor-based, so start at research-lane values.
    p.sovereignty          = 0.85;
    p.energy_efficiency    = 0.90; // low-energy signals vs heavy machinery.
    p.governance_alignment = 0.88;
    return p;
}

/// Summarize system state: aggregate experiments into a SevenDimProfile and evidence flags.
/// This function is AI-chat-friendly: it produces a compact snapshot that downstream
/// tools can serialize and reason over.
ResearchSnapshot summarize_system_state(const std::vector<ExperimentResearchObject>& experiments,
                                        const BugsLifeRiskWeights& weights,
                                        double max_risk_of_harm_threshold) {
    ResearchSnapshot snapshot{};
    snapshot.experiments = experiments;

    // Aggregate K/E/R from experiments.
    double sum_knowledge = 0.0;
    double sum_eco       = 0.0;
    double sum_risk      = 0.0;
    std::size_t count    = experiments.size();

    for (const auto& exp : experiments) {
        double vt = compute_bugs_life_residual(exp.risk_coords, weights);
        // Map residual to Risk-of-harm in [0,1] via a simple saturation.
        double r = clamp01(vt / max_risk_of_harm_threshold);
        // Use conceptual K/E values for BugsLife
        double k = 0.90;
        double e = 0.92;

        sum_knowledge += k;
        sum_eco       += e;
        sum_risk      += r;
    }

    double avg_k = count > 0 ? sum_knowledge / static_cast<double>(count) : 0.0;
    double avg_e = count > 0 ? sum_eco       / static_cast<double>(count) : 0.0;
    double avg_r = count > 0 ? sum_risk      / static_cast<double>(count) : 0.0;

    snapshot.system_profile = derive_profile_from_ker(avg_k, avg_e, avg_r);

    // Evidence flags: domain performance and energy neutrality require at least one
    // experiment with good metrics and low residual; safety, sovereignty, explainability
    // are set conservatively to false until external proofs are attached.
    bool domain_ok = false;
    bool energy_ok = false;

    for (const auto& exp : experiments) {
        double vt = compute_bugs_life_residual(exp.risk_coords, weights);
        double r  = clamp01(vt / max_risk_of_harm_threshold);

        if (exp.metrics.mean_pest_reduction >= 0.3 &&
            exp.metrics.p_value_effect < 0.05 &&
            r <= max_risk_of_harm_threshold) {
            domain_ok = true;
        }

        // Energy neutrality: here modeled as kg poisons avoided outweighing signal burden.
        if (exp.metrics.kg_poison_avoided_per_day > 0.1 &&
            exp.metrics.avg_noise_level <= 0.5 &&
            exp.metrics.avg_odor_intensity <= 0.5) {
            energy_ok = true;
        }
    }

    snapshot.evidence_flags.domain_performance_ok  = domain_ok;
    snapshot.evidence_flags.energy_neutral_or_renew = energy_ok;
    snapshot.evidence_flags.safety_case_documented  = false;
    snapshot.evidence_flags.sovereignty_compliant   = false;
    snapshot.evidence_flags.explainable_and_audited = false;

    return snapshot;
}

/// Pretty-print a ResearchSnapshot for human auditors and documentation.
void print_snapshot(const ResearchSnapshot& snapshot) {
    const SevenDimProfile& p = snapshot.system_profile;
    const SystemEvidenceFlags& e = snapshot.evidence_flags;

    std::cout << "BugsLife System Research Snapshot\n";
    std::cout << "---------------------------------\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "KnowledgeFactor       : " << p.knowledge_factor << "\n";
    std::cout << "EcoImpact             : " << p.eco_impact << "\n";
    std::cout << "RiskOfHarm            : " << p.risk_of_harm << "\n";
    std::cout << "Robustness            : " << p.robustness << "\n";
    std::cout << "Sovereignty           : " << p.sovereignty << "\n";
    std::cout << "EnergyEfficiency      : " << p.energy_efficiency << "\n";
    std::cout << "GovernanceAlignment   : " << p.governance_alignment << "\n\n";

    std::cout << "Eligibility Evidence Flags\n";
    std::cout << "  DomainPerformanceOK    : " << (e.domain_performance_ok ? "true" : "false") << "\n";
    std::cout << "  SafetyCaseDocumented   : " << (e.safety_case_documented ? "true" : "false") << "\n";
    std::cout << "  SovereigntyCompliant   : " << (e.sovereignty_compliant ? "true" : "false") << "\n";
    std::cout << "  EnergyNeutralOrRenew   : " << (e.energy_neutral_or_renew ? "true" : "false") << "\n";
    std::cout << "  ExplainableAndAudited  : " << (e.explainable_and_audited ? "true" : "false") << "\n\n";

    std::cout << "Experiments (" << snapshot.experiments.size() << ")\n";
    for (const auto& exp : snapshot.experiments) {
        std::cout << "- ID: " << exp.id.category << "::" << exp.id.name
                  << " v" << exp.id.version << "\n";
        std::cout << "  Corridor ID: " << exp.corridor_id << "\n";
        std::cout << "  Metrics:\n";
        std::cout << "    mean_pest_reduction       : " << exp.metrics.mean_pest_reduction << "\n";
        std::cout << "    p_value_effect            : " << exp.metrics.p_value_effect << "\n";
        std::cout << "    kg_poison_avoided_per_day : " << exp.metrics.kg_poison_avoided_per_day << "\n";
        std::cout << "    non_target_incidents/mo   : " << exp.metrics.non_target_incidents_per_month << "\n";
        std::cout << "    avg_noise_level           : " << exp.metrics.avg_noise_level << "\n";
        std::cout << "    avg_odor_intensity        : " << exp.metrics.avg_odor_intensity << "\n";
        std::cout << "  Gate Impact Flags:\n";
        std::cout << "    domain_performance_ok     : "
                  << (exp.gate_impact.domain_performance_ok ? "true" : "false") << "\n";
        std::cout << "    safety_case_documented    : "
                  << (exp.gate_impact.safety_case_documented ? "true" : "false") << "\n";
        std::cout << "    sovereignty_compliant     : "
                  << (exp.gate_impact.sovereignty_compliant ? "true" : "false") << "\n";
        std::cout << "    energy_neutral_or_renew   : "
                  << (exp.gate_impact.energy_neutral_or_renew ? "true" : "false") << "\n";
        std::cout << "    explainable_and_audited   : "
                  << (exp.gate_impact.explainable_and_audited ? "true" : "false") << "\n";
        std::cout << "\n";
    }
}

/// Example main wiring: construct a couple of research-lane experiments and print snapshot.
/// In Prometheus-Praxis this would be invoked by a governance CLI or testbed harness.
int main() {
    BugsLifeRiskWeights weights{
        0.6, 0.4, 0.3,   // noise
        0.5, 0.4, 1.0,   // odor (toxicity weighted high)
        0.3, 0.9, 0.3,   // light / laser
        1.0, 0.8, 0.9,   // toxicity / bioaccumulation
        0.4, 0.4, 0.5    // disturbance / habitat sensitivity
    };

    // Research-lane deployment: strong poison avoidance, moderate signals.
    ExperimentResearchObject exp1{
        ResearchObjectId{"experiment", "bugs_life_corridor_alpha", "2026.08"},
        "phx_downtown_corridor_A",
        ExperimentMetrics{
            0.35,    // mean_pest_reduction
            0.03,    // p_value_effect
            0.45,    // kg_poison_avoided_per_day
            0.10,    // non_target_incidents_per_month
            0.4,     // avg_noise_level
            0.5      // avg_odor_intensity
        },
        SystemEvidenceFlags{
            true,    // domain_performance_ok
            false,
            false,
            true,    // energy_neutral_or_renew (conceptual)
            false
        },
        BugsLifeRiskCoordinates{
            0.4, 0.3, 0.2,  // noise
            0.5, 0.4, 0.2,  // odor
            0.3, 0.2, 0.2,  // light / laser
            0.1, 0.2, 0.1,  // toxicity
            0.3, 0.3, 0.4   // disturbance / sensitivity
        }
    };

    // Second deployment: higher disturbance, used to test corridor limits.
    ExperimentResearchObject exp2{
        ResearchObjectId{"experiment", "bugs_life_corridor_beta", "2026.08"},
        "phx_industrial_corridor_B",
        ExperimentMetrics{
            0.40,
            0.02,
            0.60,
            0.15,
            0.6,   // noise closer to upper corridor band
            0.6
        },
        SystemEvidenceFlags{
            true,
            false,
            false,
            true,
            false
        },
        BugsLifeRiskCoordinates{
            0.6, 0.5, 0.3,
            0.6, 0.5, 0.3,
            0.4, 0.3, 0.2,
            0.1, 0.3, 0.2,
            0.5, 0.5, 0.6
        }
    };

    std::vector<ExperimentResearchObject> experiments{exp1, exp2};

    double max_risk_of_harm_threshold = 0.25; // Phoenix-style threshold

    ResearchSnapshot snapshot = summarize_system_state(experiments, weights, max_risk_of_harm_threshold);
    print_snapshot(snapshot);

    return 0;
}

} // namespace bugs_life
} // namespace eco
} // namespace ppx
