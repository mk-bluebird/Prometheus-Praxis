// File: cpp/eco_restoration/aln_policy_composition_and_ecosystem_continuity.cpp
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

namespace praxis {
namespace eco {

// ----------------------------------------------------------
// 35. ALN policy composition over shared resource (electrode bandwidth)
// ----------------------------------------------------------
//
// We model two ALN shards:
//   S_lab  : labor psych continuity shard
//   S_eco  : eco-restoration shard
// both constraining a shared resource "electrode_bandwidth" (BW).
//
// Each shard has invariants:
//   I_lab(BW, ...)  = true  if labor psych continuity constraints are satisfied.
//   I_eco(BW, ...)  = true  if eco-restoration constraints are satisfied.
//
// Composition operator ⊗:
//   S_comp = S_lab ⊗ S_eco
//   Invariants: I_comp = I_lab ∧ I_eco.
//
// Safety proof sketch:
//   - Assume Rust kernels K_lab and K_eco implement S_lab and S_eco, respectively,
//     each Kani-verified to enforce I_lab and I_eco for all executions that
//     fairly access BW.
//   - Under fairness (each shard’s kernel gets scheduled without starvation),
//     any composite execution path respects both invariants, hence I_comp holds.

struct BandwidthUsage {
    double bw_lab; // bandwidth requested by labor psych module
    double bw_eco; // bandwidth requested by eco-restoration module
    double bw_max; // physical maximum electrode bandwidth
};

struct LabInvariants {
    double bw_lab_cap;      // max allowed lab bandwidth
    bool   continuity_safe; // other psych continuity flags (abstract)
};

struct EcoInvariants {
    double bw_eco_cap;      // max allowed eco-restoration bandwidth
    bool   eco_safe;        // other eco-restoration flags (abstract)
};

bool I_lab(const BandwidthUsage& bw, const LabInvariants& inv) {
    bool bw_ok   = bw.bw_lab <= inv.bw_lab_cap && bw.bw_lab + bw.bw_eco <= bw.bw_max;
    bool cont_ok = inv.continuity_safe;
    return bw_ok && cont_ok;
}

bool I_eco(const BandwidthUsage& bw, const EcoInvariants& inv) {
    bool bw_ok   = bw.bw_eco <= inv.bw_eco_cap && bw.bw_lab + bw.bw_eco <= bw.bw_max;
    bool eco_ok  = inv.eco_safe;
    return bw_ok && eco_ok;
}

// Composition operator ⊗ defined as conjunction of invariants.
bool I_comp(const BandwidthUsage& bw,
            const LabInvariants& lab,
            const EcoInvariants& eco) {
    return I_lab(bw, lab) && I_eco(bw, eco);
}

// Fairness assumption:
//   Over any finite time horizon, both K_lab and K_eco are scheduled enough
//   to enforce their invariants whenever BW usage changes.
//
// Under this assumption, if each kernel is Kani-verified safe (i.e., for all
// program states and inputs, K_lab ⇒ I_lab and K_eco ⇒ I_eco), then the
// composed system K_comp (interleaving K_lab and K_eco calls) ensures I_comp.
//
// In C++ here we demonstrate the conjunction property directly.

void print_composition_check(const BandwidthUsage& bw,
                             const LabInvariants& lab,
                             const EcoInvariants& eco) {
    bool lab_ok  = I_lab(bw, lab);
    bool eco_ok  = I_eco(bw, eco);
    bool comp_ok = I_comp(bw, lab, eco);

    std::cout << "ALN policy composition over electrode bandwidth:\n";
    std::cout << "  bw_lab=" << bw.bw_lab
              << ", bw_eco=" << bw.bw_eco
              << ", bw_max=" << bw.bw_max << "\n";
    std::cout << "  I_lab holds? " << (lab_ok ? "YES" : "NO") << "\n";
    std::cout << "  I_eco holds? " << (eco_ok ? "YES" : "NO") << "\n";
    std::cout << "  I_comp = I_lab ∧ I_eco holds? " << (comp_ok ? "YES" : "NO") << "\n\n";
}

// ----------------------------------------------------------
// 36. Continuity metric Ψ for ecosystem resilience
// ----------------------------------------------------------
//
// We propose a scalar metric Ψ that quantifies ecosystem continuity of a corridor,
// incorporating biodiversity (B), canopy cover (C), and soil moisture (M).
//
// Example definition:
//   B ∈ [0,1] : normalized biodiversity index
//   C ∈ [0,1] : normalized canopy cover fraction
//   M ∈ [0,1] : normalized soil moisture index
//
//   Ψ = w_B * B + w_C * C + w_M * M
//
// with weights w_B, w_C, w_M ≥ 0, Σ w = 1.
//
// Integration with psych continuity:
//   Aggregate RoH_corridor can be modulated by Ψ so that higher Ψ reduces
//   effective RoH, reflecting that more resilient ecosystems damp stress:
//
//   RoH_eff = RoH_corridor * (1 - α_Ψ * Ψ)
//
// where α_Ψ ∈ [0,1] controls impact. ALN invariants can then be extended:
//
//   RoH_eff ≤ 0.30
//
// ensuring that investment in ecosystem continuity (higher Ψ) contributes
// directly to lowering RoH and improving psych continuity.

struct EcosystemState {
    double B; // biodiversity
    double C; // canopy cover
    double M; // soil moisture
};

struct PsiParams {
    double w_B;
    double w_C;
    double w_M;
    double alpha_psi;
};

double psi_metric(const EcosystemState& e, const PsiParams& p) {
    double psi = p.w_B * e.B + p.w_C * e.C + p.w_M * e.M;
    if (psi < 0.0) psi = 0.0;
    if (psi > 1.0) psi = 1.0;
    return psi;
}

double effective_roh(double roh_corridor,
                     const EcosystemState& e,
                     const PsiParams& p) {
    double psi = psi_metric(e, p);
    double factor = 1.0 - p.alpha_psi * psi;
    if (factor < 0.0) factor = 0.0;
    return clamp01(roh_corridor * factor);
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 35. ALN policy composition demo.
    BandwidthUsage bw{30.0, 20.0, 60.0}; // lab=30, eco=20, max=60
    LabInvariants lab{35.0, true};
    EcoInvariants eco{30.0, true};
    print_composition_check(bw, lab, eco);

    // 36. Ecosystem continuity metric Ψ integration demo.
    EcosystemState eco_state{
        0.7, // B: fairly high biodiversity
        0.6, // C: substantial canopy cover
        0.5  // M: moderate soil moisture
    };

    PsiParams psi_params{
        0.4,  // w_B
        0.4,  // w_C
        0.2,  // w_M
        0.6   // alpha_psi: strong coupling between Ψ and RoH
    };

    double roh_corridor = 0.35; // baseline corridor RoH (unsafe)
    double psi = psi_metric(eco_state, psi_params);
    double roh_eff = effective_roh(roh_corridor, eco_state, psi_params);

    std::cout << "Ecosystem continuity metric Ψ and psych continuity:\n";
    std::cout << "  Ψ = w_B * B + w_C * C + w_M * M = " << psi << "\n";
    std::cout << "  Baseline corridor RoH=" << roh_corridor
              << ", effective RoH_eff=" << roh_eff << "\n";
    std::cout << "  With ALN invariant RoH_eff <= 0.30, improving biodiversity, canopy cover,\n"
              << "  and soil moisture (higher Ψ) directly contributes to lowering aggregate RoH\n"
              << "  and strengthening psych continuity in the Phoenix corridor.\n";

    return 0;
}

} // namespace eco
} // namespace praxis
