// File: cpp/tools/ota_invariant_gate_and_electrode_stochastic_control.cpp
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

namespace praxis {
namespace tools {

// ----------------------------------------------------------
// 37. Verifying the OTA invariant gate (soundness & completeness)
// ----------------------------------------------------------
//
// We model ALN shards as trees of invariants. An OTA update proposes a new
// shard set S_new to replace S_old. The CI/OTA gate must reject any update
// that relaxes a core invariant.
//
// Let core invariants be represented as key → value pairs, e.g.:
//   "RoH_ceiling"           : "RoH<=0.30"
//   "NonNegCapDelta"        : "capability_delta>=0.0"
//   "RequiresReliabilityToken": "SNR>12dB && drift<2%/hr"
// etc.
//
// Decision procedure over diff:
//
//   For each core invariant key k:
//     let v_old = S_old[k], v_new = S_new[k].
//
//   Define Safe(k) as:
//     Safe(k) := (v_new logically implies v_old)
//
//   Gate decision formula:
//
//     AcceptUpdate := ∧_{k ∈ Core} Safe(k)
//
//   If AcceptUpdate = true, OTA passes; otherwise it is rejected.
//
// Under an implication check that is sound and complete for the invariant
// language, and assuming no hash collisions in shard content hashes,
// the gate is sound (no unsafe update passes) and complete (all safe updates pass).

struct Invariant {
    std::string key;
    std::string value_repr; // canonical textual representation
};

struct ShardTree {
    std::vector<Invariant> invariants;
};

struct CoreInvariant {
    std::string key;
};

bool logically_implies(const std::string& new_repr,
                       const std::string& old_repr) {
    // Placeholder implication checker:
    // In production, this would parse the invariant expressions and check
    // that new_repr ⇒ old_repr holds (e.g., via SMT). Here, we approximate
    // a simple pattern: new_repr must be identical or strictly stronger.
    //
    // For core examples like "RoH<=0.28" vs "RoH<=0.30", we'd parse numeric
    // thresholds and compare. Here we just use string equality as a safe
    // but conservative proxy.
    return new_repr == old_repr;
}

std::string lookup_value(const ShardTree& tree, const std::string& key) {
    for (const auto& inv : tree.invariants) {
        if (inv.key == key) {
            return inv.value_repr;
        }
    }
    return "";
}

bool ota_gate_accepts(const ShardTree& old_tree,
                      const ShardTree& new_tree,
                      const std::vector<CoreInvariant>& core) {
    for (const auto& c : core) {
        std::string v_old = lookup_value(old_tree, c.key);
        std::string v_new = lookup_value(new_tree, c.key);
        // If the invariant is missing or does not imply the old one, reject.
        if (v_old.empty() || v_new.empty()) {
            return false;
        }
        if (!logically_implies(v_new, v_old)) {
            return false;
        }
    }
    return true;
}

// Soundness and completeness sketch (commented, for Rust+Kani model checker):
//
// Assumptions:
//   - No hash collisions: each shard tree hash uniquely identifies content.
//   - logically_implies() is sound and complete for the invariant language.
//   - Core invariants list is correct and stable.
//
// Soundness:
//   For any update S_new that relaxes a core invariant, there exists some k
//   such that ¬Safe(k). Then AcceptUpdate = ∧ Safe(k) is false, and gate
//   rejects S_new. Thus no unsafe update passes.
//
// Completeness:
//   For any update S_new that preserves or strengthens all core invariants,
//   logically_implies(v_new, v_old) holds for all k. Then AcceptUpdate is true,
//   and gate accepts S_new. Thus all safe updates pass.
//
// Rust+Kani harness (conceptual):
//
// #[kani::proof]
// fn kani_prove_ota_gate_sound_complete() {
//     let old_tree = kani::any::<ShardTree>();
//     let new_tree = kani::any::<ShardTree>();
//     let core      = kani::any::<Vec<CoreInvariant>>();
//
//     // Assume core invariants present in old_tree and logically_implies is sound.
//     // For each core invariant, if new_tree value does not imply old_tree value,
//     // then ota_gate_accepts must be false.
//     // Conversely, if all imply, ota_gate_accepts must be true.
// }

// ----------------------------------------------------------
// 38. Stochastic control of electrode recalibration (CTMC + HJB)
// ----------------------------------------------------------
//
// We model electrode calibration state as a continuous-time Markov chain
// with states:
//   CALIBRATED (C), DRIFTING (D), FAILED (F).
//
// Transition rates (without control):
//   q_CD : rate from C to D (start drift)
//   q_DF : rate from D to F (failure)
//   q_DC : spontaneous recovery (e.g., minor self-correction)
//
// Control u(t): recalibration actions that move state from D or F to C,
// incurring cost and reducing psych-risk estimation error.
//
// Objective: minimize expected cumulative cost:
//   J = E[ ∫_0^∞ ( psych_risk_error(X_t) + c(u_t) ) dt ]
//
// Hamilton–Jacobi–Bellman (HJB) equation for value function V(s):
//   0 = min_u { instant_cost(s,u) + Σ_{s'} q_{s,s'}(u) [ V(s') - V(s) ] }
//
// where q_{s,s'}(u) are controlled transition rates.

enum class CalState {
    CALIBRATED,
    DRIFTING,
    FAILED
};

std::string to_string(CalState s) {
    switch (s) {
        case CalState::CALIBRATED: return "CALIBRATED";
        case CalState::DRIFTING:   return "DRIFTING";
        case CalState::FAILED:     return "FAILED";
    }
    return "UNKNOWN";
}

struct CTMCParams {
    double q_CD;  // base rate C -> D
    double q_DF;  // base rate D -> F
    double q_DC;  // base rate D -> C (self-correction)
    double q_FC;  // base rate F -> C (recalibration)
};

double psych_risk_error(CalState s) {
    switch (s) {
        case CalState::CALIBRATED: return 0.01;
        case CalState::DRIFTING:   return 0.05;
        case CalState::FAILED:     return 0.20;
    }
    return 0.20;
}

double control_cost(double u) {
    // Simple quadratic control cost.
    return 0.5 * u * u;
}

// For a fixed control policy u(s), the HJB equations for each state s:
//
// Let V_C, V_D, V_F be value functions.
//
// For CALIBRATED:
//   0 = instant_cost(C, u_C) + q_CD(u_C) (V_D - V_C)
//
// For DRIFTING:
//   0 = instant_cost(D, u_D) + q_DC(u_D) (V_C - V_D) + q_DF(u_D) (V_F - V_D)
//
// For FAILED:
//   0 = instant_cost(F, u_F) + q_FC(u_F) (V_C - V_F)
//
// Here instant_cost(s, u) = psych_risk_error(s) + control_cost(u).
//
// We can discretize control u(s) to a small set (e.g., {0, u_max}) and solve for V.

struct HJBValue {
    double V_C;
    double V_D;
    double V_F;
};

HJBValue solve_hjb_fixed_policy(const CTMCParams& p,
                                double u_C,
                                double u_D,
                                double u_F) {
    // Controlled rates:
    double q_CD_u = p.q_CD + u_C;  // recalibration in C may slightly affect drift rate
    double q_DC_u = p.q_DC + u_D;  // more recalibration in D increases D->C
    double q_DF_u = p.q_DF;        // assume unaffected by u_D
    double q_FC_u = p.q_FC + u_F;  // recalibration in F drives F->C

    // instant costs:
    double c_C = psych_risk_error(CalState::CALIBRATED) + control_cost(u_C);
    double c_D = psych_risk_error(CalState::DRIFTING)   + control_cost(u_D);
    double c_F = psych_risk_error(CalState::FAILED)     + control_cost(u_F);

    // HJB system (steady-state, discount rate ~0):
    // 0 = c_C + q_CD_u (V_D - V_C)
    // 0 = c_D + q_DC_u (V_C - V_D) + q_DF_u (V_F - V_D)
    // 0 = c_F + q_FC_u (V_C - V_F)
    //
    // Solve linear system AX = b where X = [V_C, V_D, V_F].

    // Rearranged:
    // C: c_C + q_CD_u V_D - q_CD_u V_C = 0
    // D: c_D + q_DC_u V_C + q_DF_u V_F - (q_DC_u + q_DF_u) V_D = 0
    // F: c_F + q_FC_u V_C - q_FC_u V_F = 0

    double A[3][3] = {
        {-q_CD_u,    q_CD_u,        0.0},
        { q_DC_u, -(q_DC_u+q_DF_u),  q_DF_u},
        { q_FC_u,    0.0,        -q_FC_u}
    };
    double b[3] = {-c_C, -c_D, -c_F};

    // Solve 3x3 using simple Gaussian elimination.
    double M[3][4] = {
        {A[0][0], A[0][1], A[0][2], b[0]},
        {A[1][0], A[1][1], A[1][2], b[1]},
        {A[2][0], A[2][1], A[2][2], b[2]}
    };

    for (int i = 0; i < 3; ++i) {
        // Pivot
        double pivot = M[i][i];
        if (std::fabs(pivot) < 1e-12) continue;
        for (int j = i; j < 4; ++j) {
            M[i][j] /= pivot;
        }
        // Eliminate
        for (int k = 0; k < 3; ++k) {
            if (k == i) continue;
            double factor = M[k][i];
            for (int j = i; j < 4; ++j) {
                M[k][j] -= factor * M[i][j];
            }
        }
    }

    HJBValue V{M[0][3], M[1][3], M[2][3]};
    return V;
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 37. OTA invariant gate demo.
    ShardTree old_tree{
        {
            {"RoH_ceiling", "RoH<=0.30"},
            {"NonNegCapDelta", "capability_delta>=0.0"}
        }
    };
    ShardTree new_tree_safe{
        {
            {"RoH_ceiling", "RoH<=0.30"},
            {"NonNegCapDelta", "capability_delta>=0.0"}
        }
    };
    ShardTree new_tree_unsafe{
        {
            {"RoH_ceiling", "RoH<=0.35"}, // relaxed!
            {"NonNegCapDelta", "capability_delta>=0.0"}
        }
    };
    std::vector<CoreInvariant> core{
        {"RoH_ceiling"},
        {"NonNegCapDelta"}
    };

    bool accept_safe   = ota_gate_accepts(old_tree, new_tree_safe, core);
    bool accept_unsafe = ota_gate_accepts(old_tree, new_tree_unsafe, core);

    std::cout << "OTA invariant gate decision:\n";
    std::cout << "  Safe update accepted? " << (accept_safe ? "YES" : "NO") << "\n";
    std::cout << "  Unsafe update accepted? " << (accept_unsafe ? "YES" : "NO") << "\n\n";

    // 38. Stochastic electrode recalibration control demo.
    CTMCParams params{
        0.01, // q_CD
        0.02, // q_DF
        0.005,// q_DC
        0.03  // q_FC
    };

    double u_C = 0.0;  // no extra action in CALIBRATED
    double u_D = 0.05; // moderate recalibration in DRIFTING
    double u_F = 0.10; // strong recalibration in FAILED

    HJBValue V = solve_hjb_fixed_policy(params, u_C, u_D, u_F);

    std::cout << "Stochastic control of electrode recalibration (HJB values):\n";
    std::cout << "  V(CALIBRATED)=" << V.V_C << "\n";
    std::cout << "  V(DRIFTING)="   << V.V_D << "\n";
    std::cout << "  V(FAILED)="     << V.V_F << "\n";
    std::cout << "  These value estimates guide optimal scheduling of recalibration\n"
              << "  actions to minimize psych-risk estimation error under maintenance\n"
              << "  budget constraints, by comparing V(D) and V(F) against the cost of\n"
              << "  driving the chain back to CALIBRATED.\n";

    return 0;
}

} // namespace tools
} // namespace praxis
