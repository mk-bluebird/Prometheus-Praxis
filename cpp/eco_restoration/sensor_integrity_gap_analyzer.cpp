// File: cpp/eco_restoration/sensor_integrity_gap_analyzer.cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace praxis {
namespace eco {

// High-level requirement categories inferred from 0x20260729PHXCHATLABORPSYCHCONTINUITY.
enum class Requirement {
    LaborPsychMetricIntegrity,
    HealthcareContinuityTrigger,
    SystemInstabilityCeiling,
    CapabilityRollbackPrevention,
    ElectrodeReliabilityPrecondition
};

// Status of alignment between hex standard and ALN/contract implementation.
enum class GapSeverity {
    None,
    Low,
    Moderate,
    High,
    Critical
};

struct GapRecord {
    Requirement requirement;
    GapSeverity severity;
    std::string description;
    std::string aln_mapping;
    std::string exemplar_non_compliant;
    std::string risk;
};

// Simple eco-restoration impact summary.
struct ImpactScore {
    double knowledge_factor;   // how complete/grounded the specification is
    double eco_impact_value;   // how eco/health positive the current closure is
};

std::string to_string(Requirement r) {
    switch (r) {
        case Requirement::LaborPsychMetricIntegrity:
            return "LaborPsychMetricIntegrity";
        case Requirement::HealthcareContinuityTrigger:
            return "HealthcareContinuityTrigger";
        case Requirement::SystemInstabilityCeiling:
            return "SystemInstabilityCeiling";
        case Requirement::CapabilityRollbackPrevention:
            return "CapabilityRollbackPrevention";
        case Requirement::ElectrodeReliabilityPrecondition:
            return "ElectrodeReliabilityPrecondition";
    }
    return "UnknownRequirement";
}

std::string to_string(GapSeverity s) {
    switch (s) {
        case GapSeverity::None:     return "None";
        case GapSeverity::Low:      return "Low";
        case GapSeverity::Moderate: return "Moderate";
        case GapSeverity::High:     return "High";
        case GapSeverity::Critical: return "Critical";
    }
    return "UnknownSeverity";
}

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Construct the gap records directly from your narrative specification.
std::vector<GapRecord> inferred_gap_space() {
    std::vector<GapRecord> v;

    v.push_back(GapRecord{
        Requirement::LaborPsychMetricIntegrity,
        GapSeverity::Critical,
        "No explicit link between psych metric validity and electrode reliability in ALN shards or contracts.",
        "ALN: missing invariant binding psych_risk_profile to sensor integrity tokens.",
        "Continuity decisions made solely on psych_risk scores without validating underlying sensor reliability.",
        "Decisions about healthcare continuity and labor status can be made on corrupted or noisy data, "
        "undermining all continuity guarantees and exposing hosts to misclassified risk."
    });

    v.push_back(GapRecord{
        Requirement::HealthcareContinuityTrigger,
        GapSeverity::High,
        "Contracts use ambiguous language instead of referencing machine-checkable psych_risk_band triggers.",
        "Contract: lacks clauses keyed to specific psych_risk_profile bands and dwell times.",
        "\"The system shall ensure the host's overall well-being.\"",
        "Continuity protocols cannot be formally proven or enforced; obligations degrade into aspirational statements."
    });

    v.push_back(GapRecord{
        Requirement::SystemInstabilityCeiling,
        GapSeverity::Moderate,
        "RoH <= 0.30 is defined in core ALN but may not be uniformly inherited by all labor/health shards.",
        "ALN: PerkunosNexusCore2026v1.aln encodes RoH ceiling; child shards may omit explicit inheritance.",
        "A specialized labor profile silently omits the RoH constraint and operates at higher instability.",
        "Inconsistent enforcement can open corridors where macro instability exceeds the hex ceiling, "
        "violating monotone safety evolution."
    });

    v.push_back(GapRecord{
        Requirement::CapabilityRollbackPrevention,
        GapSeverity::Low,
        "Non-negative capability delta appears well-established but still requires cross-shard checks.",
        "ALN: neurorights.catalog.v1.aln and related shards encode non-negative capability delta.",
        "Not applicable (gap is minimal but still requires verification).",
        "If not consistently enforced, niche interventions could shrink capabilities without host approval."
    });

    v.push_back(GapRecord{
        Requirement::ElectrodeReliabilityPrecondition,
        GapSeverity::Critical,
        "Electrode reliability as a formal precondition is absent; continuity can be claimed without sensor proof.",
        "ALN: no reliability_token or sensor_integrity invariant; contracts ignore calibration and drift.",
        "Healthcare continuity claim accepted based only on computed states, ignoring sensor uptime and PFAS drift.",
        "System operates on unverified inputs; this is a fatal flaw for a life-critical labor-psych governance stack."
    });

    return v;
}

// Aggregate knowledge-factor and eco-impact-value from the current gap landscape.
ImpactScore compute_impact(const std::vector<GapRecord>& gaps) {
    // Start from a neutral baseline and penalize by severity.
    double k = 0.85;
    double e = 0.85;

    for (const auto& g : gaps) {
        double penalty_k = 0.0;
        double penalty_e = 0.0;

        switch (g.severity) {
            case GapSeverity::None:
                break;
            case GapSeverity::Low:
                penalty_k = 0.02;
                penalty_e = 0.02;
                break;
            case GapSeverity::Moderate:
                penalty_k = 0.05;
                penalty_e = 0.05;
                break;
            case GapSeverity::High:
                penalty_k = 0.08;
                penalty_e = 0.10;
                break;
            case GapSeverity::Critical:
                penalty_k = 0.12;
                penalty_e = 0.14;
                break;
        }

        // Critical gaps on sensor integrity and labor psych metrics hurt eco-impact more strongly.
        if (g.requirement == Requirement::LaborPsychMetricIntegrity ||
            g.requirement == Requirement::ElectrodeReliabilityPrecondition) {
            penalty_e *= 1.2;
        }

        k -= penalty_k;
        e -= penalty_e;
    }

    return ImpactScore{clamp01(k), clamp01(e)};
}

// Suggest concrete remediation steps for each gap, in a form that can be ported into ALN/Rust.
std::string remediation_for(const GapRecord& g) {
    std::ostringstream oss;
    switch (g.requirement) {
        case Requirement::LaborPsychMetricIntegrity:
            oss << "Add an ALN invariant `psychmetricrequiresintegritytoken` binding psych_risk_profile to a "
                   "sensor_integrity_token emitted by a Rust `SensorIntegrityKernel`. OTAs and CI must reject any "
                   "change that allows continuity decisions when the token is missing or invalid.";
            break;
        case Requirement::HealthcareContinuityTrigger:
            oss << "Rewrite healthcare continuity contracts to reference explicit `psych_risk_band` states and "
                   "dwell times, e.g., MODERATE/HIGH for >6 hours triggers automatic labor pause and proxy "
                   "notification. Reflect these triggers in ALN (`healthcare.continuity.v1.aln`) and Rust guards.";
            break;
        case Requirement::SystemInstabilityCeiling:
            oss << "Define RoH ceiling inheritance explicitly in all ALN shards, and add CI/Kani checks that any "
                   "new shard or profile must preserve RoH<=0.30 under all safe verdicts.";
            break;
        case Requirement::CapabilityRollbackPrevention:
            oss << "Extend Kani harnesses over all nanoswarm and labor kernels to prove `proposedcapabilitydelta>=0` "
                   "in every admissible path, and refuse OTA updates that introduce negative deltas.";
            break;
        case Requirement::ElectrodeReliabilityPrecondition:
            oss << "Introduce a `reliability_token` schema in ALN and a corresponding Rust `SensorIntegrityKernel` "
                   "that encodes uptime, calibration error, and PFAS status. Make continuity and labor-psych "
                   "decisions conditional on the presence of a valid token.";
            break;
    }
    return oss.str();
}

// Print a tabular gap analysis aligned with the narrative report.
void print_gap_analysis(const std::vector<GapRecord>& gaps,
                        const ImpactScore& impact) {
    std::cout << "Codifying Sensor Integrity: Specification Gap Analysis "
                 "vs hex standard 0x20260729PHXCHATLABORPSYCHCONTINUITY\n\n";

    std::cout << std::left << std::setw(32) << "Requirement"
              << std::setw(12) << "Severity"
              << "Gap / Risk\n";
    std::cout << std::string(96, '-') << "\n";

    for (const auto& g : gaps) {
        std::cout << std::left << std::setw(32) << to_string(g.requirement)
                  << std::setw(12) << to_string(g.severity)
                  << g.risk << "\n";
    }

    std::cout << "\nDetailed mapping:\n";
    for (const auto& g : gaps) {
        std::cout << "\n[" << to_string(g.requirement) << "]\n";
        std::cout << "Description: " << g.description << "\n";
        std::cout << "ALN/Contract mapping: " << g.aln_mapping << "\n";
        std::cout << "Exemplar non-compliant: " << g.exemplar_non_compliant << "\n";
        std::cout << "Remediation: " << remediation_for(g) << "\n";
    }

    std::cout << "\nSummary scores:\n";
    std::cout << "  Knowledge-factor (K): " << impact.knowledge_factor << "\n";
    std::cout << "  Eco-impact value (E): " << impact.eco_impact_value << "\n";

    std::cout << "\nInterpretation:\n";
    if (impact.eco_impact_value >= 0.9 && impact.knowledge_factor >= 0.9) {
        std::cout << "- Governance and contracts are nearly aligned with the hex standard; remaining work is\n"
                     "  refinement of sensor integrity proofs and continuity wording.\n";
    } else if (impact.eco_impact_value >= 0.7) {
        std::cout << "- Core neurorights and capability invariants are in place, but missing sensor-integrity and\n"
                     "  trigger formalization create material compliance risk. Prioritize critical gaps.\n";
    } else {
        std::cout << "- Multiple high/critical gaps, especially around electrode reliability and labor psych metric\n"
                     "  integrity, undermine eco-safe operation. ALN shards, Rust kernels, and contracts must be\n"
                     "  upgraded before treating the hex standard as enforceable law.\n";
    }
}

// Entry point: run the gap analysis over the inferred requirements.
int main() {
    auto gaps   = inferred_gap_space();
    auto impact = compute_impact(gaps);
    print_gap_analysis(gaps, impact);
    return 0;
}

} // namespace eco
} // namespace praxis
