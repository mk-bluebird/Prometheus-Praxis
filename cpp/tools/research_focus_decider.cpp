// File: cpp/tools/research_focus_decider.cpp
#include <iostream>
#include <string>
#include <vector>
#include <optional>

namespace praxis {
namespace research {

enum class FocusArea {
    GovernanceContracts,
    KalmanCalibration,
    PsychElectrodeIntersection
};

enum class AnalysisMode {
    ComplianceLegal,
    SystemsEngineeringCoupling
};

enum class OutputFormat {
    LegalTechnicalCrosswalk,
    RiskImpactSummary,
    SpecGapAnalysis
};

struct ResearchConfig {
    FocusArea       focus_area;
    AnalysisMode    analysis_mode;
    OutputFormat    output_format;
};

std::string to_string(FocusArea f) {
    switch (f) {
        case FocusArea::GovernanceContracts:
            return "ALN governance and contractual obligations";
        case FocusArea::KalmanCalibration:
            return "Kalman gain calibration logic for sensor reliability";
        case FocusArea::PsychElectrodeIntersection:
            return "Intersection of labor psych metrics and electrode data continuity";
    }
    return "unknown";
}

std::string to_string(AnalysisMode m) {
    switch (m) {
        case AnalysisMode::ComplianceLegal:
            return "Compliance-oriented analysis of healthcare continuity contracts";
        case AnalysisMode::SystemsEngineeringCoupling:
            return "Systems-engineering assessment of psych state ↔ sensor calibration coupling";
    }
    return "unknown";
}

std::string to_string(OutputFormat f) {
    switch (f) {
        case OutputFormat::LegalTechnicalCrosswalk:
            return "Legal-technical crosswalk against ALN schema and hex standard";
        case OutputFormat::RiskImpactSummary:
            return "Risk impact summary over ecohealth, neurorights, and continuity";
        case OutputFormat::SpecGapAnalysis:
            return "Specification gap analysis vs hex-anchored standard";
    }
    return "unknown";
}

// Simple scoring structure that captures "knowledge-factor" and "eco-impact"
// for each candidate focus, aligned with the Lyapunov/KER framing in Phoenix shards.
struct Score {
    double knowledge_factor;   // how well-grounded in existing ALN / CI / Cyboquatics corpus
    double eco_impact_value;   // expected improvement in ecohealth / neurorights continuity
};

struct ScoredOption {
    ResearchConfig config;
    Score          score;
};

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Heuristic scoring based on the Phoenix corridor & ALN/Lyapunov narrative:
//
// - GovernanceContracts + SpecGapAnalysis + ComplianceLegal:
//   highest leverage on locking V_{t+1} - V_t <= -alpha s_t into contracts,
//   directly improves eco-safety and labor/healthcare continuity.
// - PsychElectrodeIntersection + SystemsEngineeringCoupling:
//   strong on PFAS/electrode reliability and psych continuity, slightly lower
//   governance leverage but important for sensor-side risk.
// - KalmanCalibration alone:
//   more technical, narrower governance surface, still useful for risk but
//   slightly lower eco-impact unless coupled to ALN invariants.
Score score_config(const ResearchConfig& cfg) {
    double k = 0.80;
    double e = 0.80;

    // Base knowledge: all three live in the ALN / Phoenix shard universe.
    // We tweak based on alignment with existing Lyapunov/KER text.
    switch (cfg.focus_area) {
        case FocusArea::GovernanceContracts:
            // Directly tied to invariantkerdeployable ALN contracts and Lyapunov lemmas.
            k += 0.12;
            e += 0.12;
            break;
        case FocusArea::PsychElectrodeIntersection:
            // Uses PFAS, neurorights, and continuity corridors: good eco & neuro leverage.
            k += 0.10;
            e += 0.10;
            break;
        case FocusArea::KalmanCalibration:
            // Technical Kalman gain tuning is solid but narrower in governance leverage.
            k += 0.08;
            e += 0.06;
            break;
    }

    switch (cfg.analysis_mode) {
        case AnalysisMode::ComplianceLegal:
            // Governance/contract alignment improves enforceability of Lyapunov decrements.
            e += 0.05;
            break;
        case AnalysisMode::SystemsEngineeringCoupling:
            // Strong on coupling psych state variables to sensor calibration reliability.
            k += 0.03;
            e += 0.03;
            break;
    }

    switch (cfg.output_format) {
        case OutputFormat::SpecGapAnalysis:
            // Gap analysis vs hex standard `0x20260729PHXCHATLABORPSYCHCONTINUITY`
            // directly tightens corridors and exposes missing invariants.
            k += 0.05;
            e += 0.06;
            break;
        case OutputFormat::LegalTechnicalCrosswalk:
            // Good for mapping ALN schema to contract clauses.
            k += 0.04;
            e += 0.04;
            break;
        case OutputFormat::RiskImpactSummary:
            // Useful overview, slightly less precise for machine-checkable conditions.
            k += 0.02;
            e += 0.02;
            break;
    }

    return Score{clamp01(k), clamp01(e)};
}

ScoredOption best_option(const std::vector<ResearchConfig>& configs) {
    ScoredOption best{};
    bool init = false;
    for (const auto& cfg : configs) {
        Score s = score_config(cfg);
        if (!init || s.eco_impact_value > best.score.eco_impact_value ||
            (s.eco_impact_value == best.score.eco_impact_value &&
             s.knowledge_factor > best.score.knowledge_factor)) {
            best.config = cfg;
            best.score  = s;
            init        = true;
        }
    }
    return best;
}

// Convenience helper to construct candidate set for this prompt:
std::vector<ResearchConfig> default_candidate_space() {
    std::vector<ResearchConfig> v;

    // Option A: Governance + Compliance + Spec gap analysis (high governance leverage).
    v.push_back(ResearchConfig{
        FocusArea::GovernanceContracts,
        AnalysisMode::ComplianceLegal,
        OutputFormat::SpecGapAnalysis
    });

    // Option B: Psych/electrode intersection + Systems coupling + Spec gap.
    v.push_back(ResearchConfig{
        FocusArea::PsychElectrodeIntersection,
        AnalysisMode::SystemsEngineeringCoupling,
        OutputFormat::SpecGapAnalysis
    });

    // Option C: Kalman-centric + Systems coupling + Risk summary.
    v.push_back(ResearchConfig{
        FocusArea::KalmanCalibration,
        AnalysisMode::SystemsEngineeringCoupling,
        OutputFormat::RiskImpactSummary
    });

    // Option D: Governance + Compliance + Legal-technical crosswalk.
    v.push_back(ResearchConfig{
        FocusArea::GovernanceContracts,
        AnalysisMode::ComplianceLegal,
        OutputFormat::LegalTechnicalCrosswalk
    });

    return v;
}

void print_recommendation(const ScoredOption& opt) {
    std::cout << "Recommended research configuration:\n";
    std::cout << "  Focus area:      " << to_string(opt.config.focus_area) << "\n";
    std::cout << "  Analysis mode:   " << to_string(opt.config.analysis_mode) << "\n";
    std::cout << "  Output format:   " << to_string(opt.config.output_format) << "\n";
    std::cout << "  Knowledge-factor (K): " << opt.score.knowledge_factor << "\n";
    std::cout << "  Eco-impact value (E): " << opt.score.eco_impact_value << "\n";
    std::cout << "\n";
    std::cout << "Rationale:\n";
    if (opt.config.focus_area == FocusArea::GovernanceContracts &&
        opt.config.analysis_mode == AnalysisMode::ComplianceLegal &&
        opt.config.output_format == OutputFormat::SpecGapAnalysis) {
        std::cout
            << "- This configuration directly targets ALN governance and healthcare continuity contracts,\n"
            << "  aligning them with Lyapunov-safe corridor invariants and the hex standard "
            << "0x20260729PHXCHATLABORPSYCHCONTINUITY.\n"
            << "- A specification gap analysis allows you to encode conditions like V_{t+1}-V_t <= -alpha*s_t\n"
            << "  into machine-checkable clauses, improving ecohealth and labor-psych continuity just by\n"
            << "  tightening schema and CI constraints.\n";
    } else if (opt.config.focus_area == FocusArea::PsychElectrodeIntersection) {
        std::cout
            << "- This configuration emphasises PFAS/electrode reliability and psych-state continuity,\n"
            << "  coupling labor psych metrics to sensor calibration and continuity contracts.\n";
    } else if (opt.config.focus_area == FocusArea::KalmanCalibration) {
        std::cout
            << "- This configuration focuses on Kalman gain tuning as a technical substrate for reliable\n"
            << "  sensor corridors, which can later be lifted into ALN governance clauses.\n";
    }
}

int main() {
    auto candidates = default_candidate_space();
    auto best = best_option(candidates);
    print_recommendation(best);
    return 0;
}

} // namespace research
} // namespace praxis
