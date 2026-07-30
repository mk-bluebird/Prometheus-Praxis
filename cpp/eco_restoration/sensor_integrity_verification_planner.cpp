// File: cpp/eco_restoration/sensor_integrity_verification_planner.cpp
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

namespace praxis {
namespace eco {

// High-level verification and enforcement objectives.
enum class VerificationObjective {
    KernelLogicSafety,
    SensorProvenanceHarnesses,
    ReliabilityTokenGeneration,
    ReliabilityTokenValidation,
    ShardInvariantInheritance,
    ContractTriggerFormalization
};

enum class Priority {
    Immediate,
    High,
    Medium,
    Low
};

enum class Layer {
    RustKernel,
    KaniHarness,
    ALNShard,
    ContractText,
    CI_OTAGate
};

struct Task {
    VerificationObjective objective;
    Priority              priority;
    Layer                 layer;
    std::string           id;
    std::string           description;
    std::string           artifact_hint;   // e.g., file or module names in Prometheus-Praxis
};

std::string to_string(VerificationObjective o) {
    switch (o) {
        case VerificationObjective::KernelLogicSafety:
            return "KernelLogicSafety";
        case VerificationObjective::SensorProvenanceHarnesses:
            return "SensorProvenanceHarnesses";
        case VerificationObjective::ReliabilityTokenGeneration:
            return "ReliabilityTokenGeneration";
        case VerificationObjective::ReliabilityTokenValidation:
            return "ReliabilityTokenValidation";
        case VerificationObjective::ShardInvariantInheritance:
            return "ShardInvariantInheritance";
        case VerificationObjective::ContractTriggerFormalization:
            return "ContractTriggerFormalization";
    }
    return "UnknownObjective";
}

std::string to_string(Priority p) {
    switch (p) {
        case Priority::Immediate: return "Immediate";
        case Priority::High:      return "High";
        case Priority::Medium:    return "Medium";
        case Priority::Low:       return "Low";
    }
    return "UnknownPriority";
}

std::string to_string(Layer l) {
    switch (l) {
        case Layer::RustKernel:    return "RustKernel";
        case Layer::KaniHarness:   return "KaniHarness";
        case Layer::ALNShard:      return "ALNShard";
        case Layer::ContractText:  return "ContractText";
        case Layer::CI_OTAGate:    return "CI/OTAGate";
    }
    return "UnknownLayer";
}

// Simple eco/knowledge score to summarise how good the plan is.
struct PlanScore {
    double knowledge_factor;
    double eco_impact_value;
};

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Construct a task set directly aligned with the narrative:
// - reliability_token precondition,
// - provenance harnesses,
// - shard invariants,
// - contract triggers,
// - CI/OTA enforcement.
std::vector<Task> build_verification_plan() {
    std::vector<Task> v;

    v.push_back(Task{
        VerificationObjective::ReliabilityTokenGeneration,
        Priority::Immediate,
        Layer::RustKernel,
        "T1_RELIABILITY_TOKEN_GEN",
        "Implement SensorIntegrityKernel to run diagnostics (SNR, drift, calibration status) and emit a signed "
        "reliability_token bound to host and hardware.",
        "crates/praxis-governance-kernel/src/sensor_integrity.rs"
    });

    v.push_back(Task{
        VerificationObjective::ReliabilityTokenValidation,
        Priority::Immediate,
        Layer::RustKernel,
        "T2_RELIABILITY_TOKEN_CHECK",
        "Add runtime checks so all psych_state and healthcare continuity decisions in PraxisGovernanceKernel "
        "require a valid, non-expired reliability_token.",
        "crates/praxis-governance-kernel/src/continuity_guard.rs"
    });

    v.push_back(Task{
        VerificationObjective::SensorProvenanceHarnesses,
        Priority::High,
        Layer::KaniHarness,
        "T3_SENSOR_PROVENANCE_HARNESS",
        "Write Kani harnesses that model sensor failure modes (noise spikes, drift, loss of signal) and prove the "
        "kernel rejects invalid data, raises RoH appropriately, and blocks continuity claims without a token.",
        "crates/praxis-governance-kernel/tests/kani_sensor_provenance.rs"
    });

    v.push_back(Task{
        VerificationObjective::ContractTriggerFormalization,
        Priority::High,
        Layer::ContractText,
        "T4_CONTRACT_TRIGGER_FORMALIZATION",
        "Rewrite healthcare continuity contracts to use machine-checkable triggers (psych_risk_band + dwell time) "
        "and reference reliability_token as a mandatory precondition.",
        "docs/contracts/healthcare_continuity_phx_psych_labor.md"
    });

    v.push_back(Task{
        VerificationObjective::ShardInvariantInheritance,
        Priority::High,
        Layer::ALNShard,
        "T5_SHARD_INVARIANT_INHERITANCE",
        "Define explicit inheritance of RoH<=0.30, non-negative capability delta, neurorights floors, and "
        "sensor_integrity preconditions in all ALN shards derived from PerkunosNexusCore.",
        "aln/PerkunosNexusCore2026v1.aln; aln/healthcare.continuity.v1.aln; aln/data.labor.profile.v1.aln"
    });

    v.push_back(Task{
        VerificationObjective::KernelLogicSafety,
        Priority::Medium,
        Layer::KaniHarness,
        "T6_KERNEL_LOGIC_SAFETY",
        "Extend existing Kani proofs over PraxisGovernanceKernel to cover new branches introduced by "
        "reliability_token logic and shard-invariant checks.",
        "crates/praxis-governance-kernel/tests/kani_governance_core.rs"
    });

    v.push_back(Task{
        VerificationObjective::ShardInvariantInheritance,
        Priority::Medium,
        Layer::CI_OTAGate,
        "T7_CI_OTA_INVARIANT_GATE",
        "Configure CI/OTA gates to reject any ALN shard or Rust update that relaxes core invariants or removes "
        "sensor_integrity preconditions.",
        ".github/workflows/praxis-governance-ci.yml; tools/ota/invariant_gate.rs"
    });

    v.push_back(Task{
        VerificationObjective::ContractTriggerFormalization,
        Priority::Low,
        Layer::ALNShard,
        "T8_CONTINUITY_TRIGGER_SHARD",
        "Encode continuity trigger thresholds from refined contracts directly into healthcare.continuity.v1.aln "
        "as formal ALN conditions.",
        "aln/healthcare.continuity.v1.aln"
    });

    return v;
}

// Compute plan-level scores: fewer high/critical gaps once tasks are adopted.
PlanScore compute_plan_score(const std::vector<Task>& tasks) {
    // Base scores reflecting current state before plan adoption.
    double k = 0.70;
    double e = 0.65;

    for (const auto& t : tasks) {
        double bonus_k = 0.0;
        double bonus_e = 0.0;

        switch (t.objective) {
            case VerificationObjective::ReliabilityTokenGeneration:
            case VerificationObjective::ReliabilityTokenValidation:
                // Directly closes critical sensor integrity gaps.
                bonus_k = 0.05;
                bonus_e = 0.08;
                break;
            case VerificationObjective::SensorProvenanceHarnesses:
                bonus_k = 0.06;
                bonus_e = 0.06;
                break;
            case VerificationObjective::ShardInvariantInheritance:
                bonus_k = 0.04;
                bonus_e = 0.05;
                break;
            case VerificationObjective::ContractTriggerFormalization:
                bonus_k = 0.04;
                bonus_e = 0.04;
                break;
            case VerificationObjective::KernelLogicSafety:
                bonus_k = 0.03;
                bonus_e = 0.03;
                break;
        }

        if (t.priority == Priority::Immediate) {
            bonus_k *= 1.1;
            bonus_e *= 1.1;
        } else if (t.priority == Priority::High) {
            bonus_k *= 1.05;
            bonus_e *= 1.05;
        }

        k += bonus_k;
        e += bonus_e;
    }

    return PlanScore{clamp01(k), clamp01(e)};
}

void print_plan(const std::vector<Task>& tasks, const PlanScore& s) {
    std::cout << "Formal Verification and Runtime Enforcement Plan\n";
    std::cout << "Aligned with hex standard 0x20260729PHXCHATLABORPSYCHCONTINUITY\n\n";

    std::cout << std::left << std::setw(8)  << "ID"
              << std::setw(26) << "Objective"
              << std::setw(10) << "Priority"
              << std::setw(14) << "Layer"
              << "Artifact / Description\n";
    std::cout << std::string(100, '-') << "\n";

    for (const auto& t : tasks) {
        std::cout << std::left << std::setw(8)  << t.id
                  << std::setw(26) << to_string(t.objective)
                  << std::setw(10) << to_string(t.priority)
                  << std::setw(14) << to_string(t.layer)
                  << t.artifact_hint << " — " << t.description << "\n";
    }

    std::cout << "\nPlan scores:\n";
    std::cout << "  Knowledge-factor (K): " << s.knowledge_factor << "\n";
    std::cout << "  Eco-impact value (E): " << s.eco_impact_value << "\n";

    std::cout << "\nInterpretation:\n";
    if (s.eco_impact_value >= 0.9 && s.knowledge_factor >= 0.9) {
        std::cout << "- With this plan fully implemented, the governance stack achieves deep coupling between "
                     "ALN invariants, Rust kernels, sensor integrity, and contracts, closing critical compliance "
                     "gaps for the hex standard.\n";
    } else if (s.eco_impact_value >= 0.8) {
        std::cout << "- The proposed tasks substantially improve end-to-end safety and data provenance. "
                     "Remaining work will be in refining thresholds and additional harness coverage.\n";
    } else {
        std::cout << "- The plan is a strong first step but further expansion (e.g., more provenance scenarios, "
                     "additional shard families) will be needed to fully meet the continuity and neurorights "
                     "requirements.\n";
    }
}

int main() {
    auto tasks = build_verification_plan();
    auto score = compute_plan_score(tasks);
    print_plan(tasks, score);
    return 0;
}

} // namespace eco
} // namespace praxis
