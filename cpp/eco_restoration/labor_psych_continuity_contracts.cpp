// File: cpp/eco_restoration/labor_psych_continuity_contracts.cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <iomanip>
#include <algorithm>
#include <sstream>

namespace praxis {
namespace eco {

enum class RiskBand {
    Normal,
    Moderate,
    High
};

enum class ElectrodeStatus {
    Reliable,
    Degraded,
    Failed
};

enum class ContinuityClauseType {
    IdentityNonRollback,
    NeurorightsFloor,
    LaborCompensation,
    ElectrodeReliability,
    EcoRiskCeiling,
    RestDutyCycle
};

struct PsychStateSnapshot {
    double roh;                     // Risk-of-Harm scalar in [0,1]
    double lifeforce;               // Lifeforce / resilience scalar in [0,1]
    RiskBand band;                  // Derived band from roh and policy
    double psych_risk;              // Psych_risk signal in [0,1]
    bool rest_required;             // Whether enforced rest is required
};

struct ElectrodeTelemetry {
    double uptime_ratio;            // fraction of time electrodes are online, [0,1]
    double calibration_error;       // normalized calibration error in [0,1]
    ElectrodeStatus status;         // qualitative status
    bool pfas_present;              // whether PFAS-based materials are involved
};

struct LaborProfile {
    double duty_cycle_hours;        // hours worked in the current window
    double recovery_hours;          // hours of rest in the current window
    double eco_load_index;          // composite eco load for the labor process
    bool minors_involved;           // whether minors are involved (must be false)
};

struct ContinuityClause {
    ContinuityClauseType type;
    std::string          id;        // local identifier, e.g. "CC_IDENTITY_NON_ROLLBACK"
    std::string          description;
    bool                 satisfied;
    std::string          evidence;  // human-readable evidence trace
};

struct ContractHexStandard {
    std::string hex_anchor;                 // e.g. "0x20260729PHXCHATLABORPSYCHCONTINUITY"
    std::map<ContinuityClauseType, ContinuityClause> clauses;
};

std::string to_string(RiskBand band) {
    switch (band) {
        case RiskBand::Normal:   return "NORMAL";
        case RiskBand::Moderate: return "MODERATE";
        case RiskBand::High:     return "HIGH";
    }
    return "UNKNOWN";
}

std::string to_string(ElectrodeStatus s) {
    switch (s) {
        case ElectrodeStatus::Reliable: return "Reliable";
        case ElectrodeStatus::Degraded: return "Degraded";
        case ElectrodeStatus::Failed:   return "Failed";
    }
    return "UNKNOWN";
}

std::string to_string(ContinuityClauseType t) {
    switch (t) {
        case ContinuityClauseType::IdentityNonRollback:
            return "IdentityNonRollback";
        case ContinuityClauseType::NeurorightsFloor:
            return "NeurorightsFloor";
        case ContinuityClauseType::LaborCompensation:
            return "LaborCompensation";
        case ContinuityClauseType::ElectrodeReliability:
            return "ElectrodeReliability";
        case ContinuityClauseType::EcoRiskCeiling:
            return "EcoRiskCeiling";
        case ContinuityClauseType::RestDutyCycle:
            return "RestDutyCycle";
    }
    return "UnknownClauseType";
}

// Simple helper to clamp to [0,1].
double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Derive a risk band from RoH and psych_risk based on eco-restoration invariants.
// RoH <= 0.30 is treated as a global eco-safe ceiling; above that we are in HIGH.
RiskBand derive_risk_band(double roh, double psych_risk) {
    roh = clamp01(roh);
    psych_risk = clamp01(psych_risk);

    if (roh <= 0.20 && psych_risk <= 0.40) {
        return RiskBand::Normal;
    }
    if (roh <= 0.30 && psych_risk <= 0.80) {
        return RiskBand::Moderate;
    }
    return RiskBand::High;
}

// Classify electrode status based on uptime and calibration error.
// High uptime and low error => Reliable; moderate => Degraded; else Failed.
ElectrodeStatus derive_electrode_status(double uptime_ratio, double calibration_error) {
    uptime_ratio      = clamp01(uptime_ratio);
    calibration_error = clamp01(calibration_error);

    if (uptime_ratio >= 0.95 && calibration_error <= 0.05) {
        return ElectrodeStatus::Reliable;
    }
    if (uptime_ratio >= 0.80 && calibration_error <= 0.10) {
        return ElectrodeStatus::Degraded;
    }
    return ElectrodeStatus::Failed;
}

// Initialize the continuity contract with all expected clauses keyed to the hex standard.
ContractHexStandard make_base_contract(const std::string& hex_anchor) {
    ContractHexStandard c{hex_anchor, {}};

    c.clauses[ContinuityClauseType::IdentityNonRollback] = ContinuityClause{
        ContinuityClauseType::IdentityNonRollback,
        "CC_IDENTITY_NON_ROLLBACK",
        "Identity capabilities must not regress; any intervention must satisfy ΔIdentity >= 0.",
        false,
        ""
    };

    c.clauses[ContinuityClauseType::NeurorightsFloor] = ContinuityClause{
        ContinuityClauseType::NeurorightsFloor,
        "CC_NEURORIGHTS_FLOOR",
        "Neurorights compliance index must remain above the minimum floor during all labor and therapeutic operations.",
        false,
        ""
    };

    c.clauses[ContinuityClauseType::LaborCompensation] = ContinuityClause{
        ContinuityClauseType::LaborCompensation,
        "CC_LABOR_COMPENSATION",
        "Labor involving psych_risk must be compensated fairly, with no labor-based restrictions weaponized against the host.",
        false,
        ""
    };

    c.clauses[ContinuityClauseType::ElectrodeReliability] = ContinuityClause{
        ContinuityClauseType::ElectrodeReliability,
        "CC_ELECTRODE_RELIABILITY",
        "Continuity claims are valid only when electrode uptime and calibration meet eco-safe thresholds.",
        false,
        ""
    };

    c.clauses[ContinuityClauseType::EcoRiskCeiling] = ContinuityClause{
        ContinuityClauseType::EcoRiskCeiling,
        "CC_ECO_RISK_CEILING",
        "Global Risk-of-Harm (RoH) must remain below the eco-safe ceiling for all continuity-guaranteed operations.",
        false,
        ""
    };

    c.clauses[ContinuityClauseType::RestDutyCycle] = ContinuityClause{
        ContinuityClauseType::RestDutyCycle,
        "CC_REST_DUTY_CYCLE",
        "Labor duty cycles must be bounded and interleaved with sufficient recovery hours to avoid long-term eco and psych damage.",
        false,
        ""
    };

    return c;
}

// Evaluate the IdentityNonRollback clause based on lifeforce and risk band.
// In this simplified model, we treat lifeforce as a proxy for capabilities
// and require that it does not fall below a safe floor in any window.
void evaluate_identity_non_rollback(ContractHexStandard& c,
                                    const PsychStateSnapshot& state_before,
                                    const PsychStateSnapshot& state_after) {
    auto& clause = c.clauses[ContinuityClauseType::IdentityNonRollback];

    bool non_regress = state_after.lifeforce >= state_before.lifeforce;
    bool band_safe   = state_after.band != RiskBand::High;

    clause.satisfied = non_regress && band_safe;

    std::ostringstream oss;
    oss << "lifeforce_before=" << state_before.lifeforce
        << ", lifeforce_after=" << state_after.lifeforce
        << ", band_before=" << to_string(state_before.band)
        << ", band_after=" << to_string(state_after.band)
        << "; non_regress=" << (non_regress ? "true" : "false")
        << ", band_safe=" << (band_safe ? "true" : "false");
    clause.evidence = oss.str();
}

// Evaluate the neurorights floor using lifeforce and psych_risk.
// We treat neurorights compliance as high if lifeforce >= 0.5 and psych_risk <= 0.7.
void evaluate_neurorights_floor(ContractHexStandard& c,
                                const PsychStateSnapshot& state) {
    auto& clause = c.clauses[ContinuityClauseType::NeurorightsFloor];

    bool lifeforce_ok = state.lifeforce >= 0.5;
    bool psych_ok     = state.psych_risk <= 0.7;

    clause.satisfied = lifeforce_ok && psych_ok;

    std::ostringstream oss;
    oss << "lifeforce=" << state.lifeforce
        << ", psych_risk=" << state.psych_risk
        << "; lifeforce_ok=" << (lifeforce_ok ? "true" : "false")
        << ", psych_ok=" << (psych_ok ? "true" : "false");
    clause.evidence = oss.str();
}

// Evaluate labor compensation and eco alignment.
// We enforce that minors are never involved, duty_cycle_hours is bounded,
// and eco_load_index remains below a threshold (e.g., 0.6).
void evaluate_labor_compensation(ContractHexStandard& c,
                                 const LaborProfile& labor) {
    auto& clause = c.clauses[ContinuityClauseType::LaborCompensation];

    bool minors_ok    = !labor.minors_involved;
    bool duty_bounded = labor.duty_cycle_hours <= 10.0;
    bool eco_safe     = labor.eco_load_index <= 0.6;

    clause.satisfied = minors_ok && duty_bounded && eco_safe;

    std::ostringstream oss;
    oss << "minors_involved=" << (labor.minors_involved ? "true" : "false")
        << ", duty_cycle_hours=" << labor.duty_cycle_hours
        << ", recovery_hours=" << labor.recovery_hours
        << ", eco_load_index=" << labor.eco_load_index
        << "; minors_ok=" << (minors_ok ? "true" : "false")
        << ", duty_bounded=" << (duty_bounded ? "true" : "false")
        << ", eco_safe=" << (eco_safe ? "true" : "false");
    clause.evidence = oss.str();
}

// Evaluate electrode reliability and PFAS risk.
// Continuity is only valid when status is Reliable or Degraded with small error,
// and PFAS presence requires stricter calibration thresholds.
void evaluate_electrode_reliability(ContractHexStandard& c,
                                    const ElectrodeTelemetry& telemetry) {
    auto& clause = c.clauses[ContinuityClauseType::ElectrodeReliability];

    bool status_ok = (telemetry.status == ElectrodeStatus::Reliable) ||
                     (telemetry.status == ElectrodeStatus::Degraded);
    bool uptime_ok = telemetry.uptime_ratio >= (telemetry.pfas_present ? 0.98 : 0.95);
    bool error_ok  = telemetry.calibration_error <= (telemetry.pfas_present ? 0.03 : 0.05);

    clause.satisfied = status_ok && uptime_ok && error_ok;

    std::ostringstream oss;
    oss << "uptime_ratio=" << telemetry.uptime_ratio
        << ", calibration_error=" << telemetry.calibration_error
        << ", status=" << to_string(telemetry.status)
        << ", pfas_present=" << (telemetry.pfas_present ? "true" : "false")
        << "; status_ok=" << (status_ok ? "true" : "false")
        << ", uptime_ok=" << (uptime_ok ? "true" : "false")
        << ", error_ok=" << (error_ok ? "true" : "false");
    clause.evidence = oss.str();
}

// Evaluate global eco risk ceiling using RoH and band.
void evaluate_eco_risk_ceiling(ContractHexStandard& c,
                               const PsychStateSnapshot& state) {
    auto& clause = c.clauses[ContinuityClauseType::EcoRiskCeiling];

    bool roh_ok  = state.roh <= 0.30;
    bool band_ok = state.band != RiskBand::High;

    clause.satisfied = roh_ok && band_ok;

    std::ostringstream oss;
    oss << "roh=" << state.roh
        << ", band=" << to_string(state.band)
        << "; roh_ok=" << (roh_ok ? "true" : "false")
        << ", band_ok=" << (band_ok ? "true" : "false");
    clause.evidence = oss.str();
}

// Evaluate rest duty cycle using labor profile and psych rest_required flag.
void evaluate_rest_duty_cycle(ContractHexStandard& c,
                              const LaborProfile& labor,
                              const PsychStateSnapshot& state) {
    auto& clause = c.clauses[ContinuityClauseType::RestDutyCycle];

    double total_hours = labor.duty_cycle_hours + labor.recovery_hours;
    double rest_ratio  = (total_hours > 0.0) ? (labor.recovery_hours / total_hours) : 1.0;

    bool rest_ok   = rest_ratio >= 0.3;
    bool duty_ok   = labor.duty_cycle_hours <= 10.0;
    bool high_band_requires_rest = (state.band == RiskBand::High ? state.rest_required : true);

    clause.satisfied = rest_ok && duty_ok && high_band_requires_rest;

    std::ostringstream oss;
    oss << "duty_cycle_hours=" << labor.duty_cycle_hours
        << ", recovery_hours=" << labor.recovery_hours
        << ", rest_ratio=" << rest_ratio
        << ", band=" << to_string(state.band)
        << ", rest_required_flag=" << (state.rest_required ? "true" : "false")
        << "; rest_ok=" << (rest_ok ? "true" : "false")
        << ", duty_ok=" << (duty_ok ? "true" : "false")
        << ", high_band_requires_rest=" << (high_band_requires_rest ? "true" : "false");
    clause.evidence = oss.str();
}

// Compute a simple knowledge-factor and eco-impact score for the contract evaluation.
struct EvaluationScore {
    double knowledge_factor;
    double eco_impact_value;
};

EvaluationScore compute_scores(const ContractHexStandard& c) {
    int satisfied_count = 0;
    for (const auto& kv : c.clauses) {
        if (kv.second.satisfied) {
            ++satisfied_count;
        }
    }
    int total = static_cast<int>(c.clauses.size());

    double completeness = (total > 0) ? (static_cast<double>(satisfied_count) / total) : 1.0;

    bool identity_ok    = c.clauses.at(ContinuityClauseType::IdentityNonRollback).satisfied;
    bool neurorights_ok = c.clauses.at(ContinuityClauseType::NeurorightsFloor).satisfied;
    bool eco_ceiling_ok = c.clauses.at(ContinuityClauseType::EcoRiskCeiling).satisfied;

    double k = completeness;
    if (identity_ok && neurorights_ok && eco_ceiling_ok) {
        k += 0.10;
    }

    bool electrode_ok = c.clauses.at(ContinuityClauseType::ElectrodeReliability).satisfied;
    bool rest_ok      = c.clauses.at(ContinuityClauseType::RestDutyCycle).satisfied;

    double e = completeness;
    if (electrode_ok)   e += 0.08;
    if (rest_ok)        e += 0.06;
    if (eco_ceiling_ok) e += 0.06;

    return EvaluationScore{clamp01(k), clamp01(e)};
}

// Pretty-print the contract evaluation as a spec-gap oriented report.
void print_contract_evaluation(const ContractHexStandard& c,
                               const EvaluationScore& score) {
    std::cout << "Hex-anchored continuity standard: " << c.hex_anchor << "\n";
    std::cout << "Specification gap analysis for labor-psych continuity:\n\n";

    std::cout << std::left << std::setw(24) << "ClauseType"
              << std::setw(14) << "Satisfied"
              << "Evidence\n";
    std::cout << std::string(80, '-') << "\n";

    for (const auto& kv : c.clauses) {
        const ContinuityClause& clause = kv.second;
        std::cout << std::left << std::setw(24) << to_string(clause.type)
                  << std::setw(14) << (clause.satisfied ? "YES" : "NO")
                  << clause.evidence << "\n";
    }

    std::cout << "\nSummary scores:\n";
    std::cout << "  Knowledge-factor (K): " << score.knowledge_factor << "\n";
    std::cout << "  Eco-impact value (E): " << score.eco_impact_value << "\n";

    std::cout << "\nInterpretation:\n";
    if (score.eco_impact_value >= 0.9 && score.knowledge_factor >= 0.9) {
        std::cout << "- Current labor-psych continuity setup is strongly aligned with the hex standard,\n"
                  << "  with all core continuity and eco-safety clauses satisfied.\n";
    } else if (score.eco_impact_value >= 0.7) {
        std::cout << "- Most continuity clauses are satisfied, but some gaps remain. Focus on unsatisfied\n"
                  << "  clauses to tighten eco and neurorights protections.\n";
    } else {
        std::cout << "- Significant specification gaps exist. The contract and associated ALN/Rust hooks\n"
                  << "  should be revised to enforce identity non-rollback, neurorights floors, and\n"
                  << "  stricter eco risk ceilings.\n";
    }
}

// Demonstration main: constructs a simple scenario and runs the evaluation.
int main() {
    // Example psych state before and after an intervention or labor window.
    PsychStateSnapshot state_before{
        0.22,      // roh
        0.55,      // lifeforce
        RiskBand::Normal,
        0.40,      // psych_risk
        false      // rest_required
    };

    PsychStateSnapshot state_after{
        0.26,      // roh slightly higher but still under eco ceiling
        0.58,      // lifeforce improved
        RiskBand::Moderate,
        0.50,      // psych_risk increased but within moderate band
        true       // rest is now required due to band elevation
    };

    // Derive bands to ensure consistency.
state_before.band = derive_risk_band(state_before.roh, state_before.psych_risk);
state_after.band = derive_risk_band(state_after.roh, state_after.psych_risk);

// Example electrode telemetry for the same window.
ElectrodeTelemetry telemetry{
0.97, // uptime_ratio
0.04, // calibration_error
derive_electrode_status(0.97, 0.04), // status
true // pfas_present
};

// Example labor profile.
LaborProfile labor{
8.0, // duty_cycle_hours
4.0, // recovery_hours
0.45, // eco_load_index
false // minors_involved
};

// Build base contract keyed to the Phoenix labor-psych continuity standard.
ContractHexStandard contract =
make_base_contract("0x20260729PHXCHATLABORPSYCHCONTINUITY");

// Evaluate all clauses.
evaluate_identity_non_rollback(contract, state_before, state_after);
evaluate_neurorights_floor(contract, state_after);
evaluate_labor_compensation(contract, labor);
evaluate_electrode_reliability(contract, telemetry);
evaluate_eco_risk_ceiling(contract, state_after);
evaluate_rest_duty_cycle(contract, labor, state_after);

// Compute scores and print report.
EvaluationScore score = compute_scores(contract);
print_contract_evaluation(contract, score);

return 0;
}

} // namespace eco
} // namespace praxis
