// File: cpp/eco_restoration/corridor_optimization_and_neurorights_floor.cpp
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <limits>

namespace praxis {
namespace eco {

// ----------------------------------------------------------
// 9. Mathematical corridor optimization (integer program)
// ----------------------------------------------------------

// Hex-cell representation in the Phoenix corridor graph.
struct HexCell {
    int    id;
    double eco_capacity;   // eco-restoration capacity (e.g., cooling potential) for this cell
    double roh_base;       // baseline Risk-of-Harm in [0,1]
};

struct Edge {
    int    from;
    int    to;
    double travel_time;    // time for robots to traverse this edge
};

// Integer program (conceptual):
//
// Variables:
//   x_i ∈ {0,1}  for each hex i  (1 if selected for eco-restoration deployment)
//   y_e ∈ {0,1}  for each edge e (1 if used in the robot travel path)
//
// Maximize:
//   Σ_i eco_capacity_i * x_i
//
// Subject to:
//   Σ_e travel_time_e * y_e <= T_budget
//   roh_base_i + Δroh(x_i) <= 0.30  for all i
//   Graph connectivity / path constraints (e.g., robots can reach selected hexes)
//
// Here we encode a small heuristic solver that approximates the integer program by:
// - Greedy selection of cells by eco_capacity subject to RoH ceiling.
// - Travel-time budget treated as a simple sum over selected cells' "access times"
//   (e.g., precomputed or approximated).
//
// ALN-compatible relaxation:
// - Treat x_i ∈ [0,1] as fractional deployment intensity, and enforce the RoH ceiling
//   as an ALN invariant roh_i(x_i) <= 0.30. This linear relaxation can be verified
//   against ALN shards and Rust kernels without requiring a full MILP solver.

struct CorridorGraph {
    std::vector<HexCell> cells;
    std::vector<Edge>    edges;
};

struct CorridorSolution {
    std::vector<int> selected_cells;
    double total_capacity;
    double total_travel_time;
};

// Simple relaxation: fractional deployment intensity λ_i ∈ [0,1].
// We approximate λ_i = 1.0 for selected cells, 0.0 otherwise, but the logic
// can be extended to fractional values in a linear program.
CorridorSolution greedy_corridor_optimization(const CorridorGraph& graph,
                                              double travel_time_budget) {
    // For simplicity, assume each cell has an "access time" equal to the minimum
    // edge travel_time incident to it, or a default constant if isolated.
    std::vector<double> access_time(graph.cells.size(), 1.0);
    for (const auto& e : graph.edges) {
        if (e.from >= 0 && e.from < static_cast<int>(graph.cells.size())) {
            access_time[e.from] = std::min(access_time[e.from], e.travel_time);
        }
        if (e.to >= 0 && e.to < static_cast<int>(graph.cells.size())) {
            access_time[e.to] = std::min(access_time[e.to], e.travel_time);
        }
    }

    // Greedy: sort cells by eco_capacity/access_time ratio, subject to RoH ceiling.
    std::vector<int> indices(graph.cells.size());
    for (std::size_t i = 0; i < graph.cells.size(); ++i) {
        indices[i] = static_cast<int>(i);
    }
    std::sort(indices.begin(), indices.end(),
              [&](int a, int b) {
                  double ra = graph.cells[a].eco_capacity / access_time[a];
                  double rb = graph.cells[b].eco_capacity / access_time[b];
                  return ra > rb;
              });

    double total_cap = 0.0;
    double total_time = 0.0;
    std::vector<int> selected;

    for (int idx : indices) {
        const HexCell& cell = graph.cells[idx];
        // Enforce RoH ceiling: only select cells whose baseline RoH is already <= 0.30.
        if (cell.roh_base > 0.30) {
            continue;
        }
        double t_candidate = total_time + access_time[idx];
        if (t_candidate <= travel_time_budget) {
            selected.push_back(cell.id);
            total_cap  += cell.eco_capacity;
            total_time  = t_candidate;
        }
    }

    return CorridorSolution{selected, total_cap, total_time};
}

// ----------------------------------------------------------
// 10. Neurorights floor formalization and logging invariant
// ----------------------------------------------------------

// We model a simplified set of system variables and neurorights floor Horn clauses.
//
// Variables (abstract):
//   RoH_global      : real ∈ [0,1]
//   capability_delta: real
//   consent_flag    : bool
//   labor_restriction_enabled : bool
//   opaque_score_used        : bool
//
// Neurorights floor Horn clauses (examples):
//   1) (RoH_global > 0.30)  -> violation
//   2) (capability_delta < 0.0) -> violation
//   3) (labor_restriction_enabled = true) -> violation
//   4) (opaque_score_used = true) -> violation
//   5) (!consent_flag) -> violation
//
// In SMT form, we express invariants as universally quantified conditions:
//   ∀ state. violation(state) = false
//
// Here we encode the violation detector and invariant checker in C++ form that
// can be mirrored by Rust + SMT/Kani.

// Abstract system state.
struct SystemState {
    double RoH_global;                // [0,1]
    double capability_delta;          // change in capability index
    bool   consent_flag;              // host consent present
    bool   labor_restriction_enabled; // any labor-based restriction active
    bool   opaque_score_used;         // opaque model score used for decisions
};

// Horn-clause style violation detector.
bool neurorights_violation(const SystemState& s) {
    if (s.RoH_global > 0.30) return true;
    if (s.capability_delta < 0.0) return true;
    if (s.labor_restriction_enabled) return true;
    if (s.opaque_score_used) return true;
    if (!s.consent_flag) return true;
    return false;
}

// SMT-friendly invariant specification (commented form):
//
// Let variables:
//   RoH_global ∈ [0,1]
//   capability_delta ∈ ℝ
//   consent_flag ∈ {true,false}
//   labor_restriction_enabled ∈ {true,false}
//   opaque_score_used ∈ {true,false}
//
// Define predicate Violation(s):
//   Violation(s) := (RoH_global > 0.30) ∨
//                   (capability_delta < 0.0) ∨
//                   labor_restriction_enabled ∨
//                   opaque_score_used ∨
//                   (¬consent_flag)
//
// Neurorights floor invariant:
//   ∀ s. ¬Violation(s)
//
// This can be encoded for SMT as:
//   assert(forall s: ¬Violation(s))
// or as a set of implications:
//   RoH_global <= 0.30
//   capability_delta >= 0.0
//   ¬labor_restriction_enabled
//   ¬opaque_score_used
//   consent_flag = true

// Logging and rollback hooks:
//
// In Rust, NeurorightsFeatureGuard would:
//   - Evaluate neurorights_violation(state) on each decision.
//   - If true, append an immutable log entry on-chain (e.g., via Veritas-chain),
//     and trigger ALN-enforced rollback (e.g., revert to last known safe shard).
//
// Here we model the logging and rollback triggers abstractly.
struct ViolationLogEntry {
    SystemState state;
    std::string reason;
};

struct NeurorightsGuardResult {
    bool violation_detected;
    ViolationLogEntry log;
    bool rollback_requested;
};

NeurorightsGuardResult check_neurorights_floor(const SystemState& s) {
    NeurorightsGuardResult res{};
    res.violation_detected = neurorights_violation(s);
    if (res.violation_detected) {
        std::ostringstream oss;
        oss << "Neurorights violation: RoH_global=" << s.RoH_global
            << ", capability_delta=" << s.capability_delta
            << ", consent_flag=" << (s.consent_flag ? "true" : "false")
            << ", labor_restriction_enabled="
            << (s.labor_restriction_enabled ? "true" : "false")
            << ", opaque_score_used="
            << (s.opaque_score_used ? "true" : "false");
        res.log = ViolationLogEntry{s, oss.str()};
        res.rollback_requested = true;
    } else {
        res.rollback_requested = false;
    }
    return res;
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 9. Corridor optimization demo.
    CorridorGraph graph;
    graph.cells = {
        {0, 10.0, 0.25},
        {1,  8.0, 0.28},
        {2,  6.0, 0.32}, // above RoH ceiling, should be skipped
        {3, 12.0, 0.20},
        {4,  9.0, 0.27}
    };
    graph.edges = {
        {0, 1, 1.5},
        {1, 3, 2.0},
        {3, 4, 1.0}
    };

    double travel_budget = 4.0; // hours
    CorridorSolution sol = greedy_corridor_optimization(graph, travel_budget);

    std::cout << "Corridor optimization (relaxed integer program):\n";
    std::cout << "  Travel time budget: " << travel_budget << "\n";
    std::cout << "  Selected cells: ";
    for (int id : sol.selected_cells) {
        std::cout << id << " ";
    }
    std::cout << "\n  Total eco capacity: " << sol.total_capacity << "\n";
    std::cout << "  Total travel time: " << sol.total_travel_time << "\n\n";

    // 10. Neurorights floor demo.
    SystemState safe_state{
        0.25,   // RoH_global
        0.05,   // capability_delta
        true,   // consent_flag
        false,  // labor_restriction_enabled
        false   // opaque_score_used
    };

    SystemState bad_state{
        0.35,   // RoH_global, above ceiling
        -0.10,  // capability_delta, negative
        false,  // consent_flag
        true,   // labor restriction
        true    // opaque score used
    };

    auto safe_res = check_neurorights_floor(safe_state);
    auto bad_res  = check_neurorights_floor(bad_state);

    std::cout << "Neurorights floor check (safe state):\n";
    std::cout << "  Violation detected? "
              << (safe_res.violation_detected ? "YES" : "NO") << "\n";
    std::cout << "  Rollback requested? "
              << (safe_res.rollback_requested ? "YES" : "NO") << "\n\n";

    std::cout << "Neurorights floor check (violating state):\n";
    std::cout << "  Violation detected? "
              << (bad_res.violation_detected ? "YES" : "NO") << "\n";
    std::cout << "  Rollback requested? "
              << (bad_res.rollback_requested ? "YES" : "NO") << "\n";
    if (bad_res.violation_detected) {
        std::cout << "  Log reason: " << bad_res.log.reason << "\n";
    }

    return 0;
}

} // namespace eco
} // namespace praxis
