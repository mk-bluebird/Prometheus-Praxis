// File: ecorestorationshard/src/ker_adversarial_robustness.cpp

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

/*
 * 38. Adversarial robustness of KER triads.
 *
 * Under the non‑actuating architecture, this module specifies a minimal
 * set of integrity constraints on the daily_progress chain that prevent
 * a compromised C++ sensor or workload producer from fabricating a false
 * positive E‑increase while hiding ecological damage.
 *
 * The design assumes:
 *   - daily_progress is append‑only, indexed by monotonically increasing
 *     dates and sample sequence numbers.
 *   - Each row carries K,E,R, residual V_t, and an evidence hex stamp.
 *   - Residual monotonicity and corridor bands are the ground truth;
 *     K,E,R are derived signals.
 *
 * This file is NON‑ACTUATING. It only evaluates integrity and defines
 * a formal security game plus mitigation predicates over diagnostic data.
 */

struct ProgressRecord {
    std::string yyyymmdd;       // e.g., "20260709"
    std::string sample_id;      // chain‑local sample identifier
    std::string node_id;        // e.g., "PHX-CANAL-NODE-WL-01"
    double k;                   // Knowledge factor K in [0,1]
    double e;                   // Eco‑impact factor E in [0,1]
    double r;                   // Risk factor R in [0,1]
    double vt_before;           // Residual V_t before workload
    double vt_after;            // Residual V_t after workload
    double delta_vt;            // vt_after - vt_before
    std::string evidence_hex;   // hex stamp binding to telemetry
    std::string prior_sample_id;// pointer to previous sample in chain
};

/*
 * Minimal integrity constraints on daily_progress, expressed as static
 * predicates over sequences of ProgressRecord rows.
 *
 * C1. Temporal append‑only:
 *   - Records for a given node_id must be strictly ordered by
 *     (yyyymmdd, sample_id) lexicographically.
 *   - prior_sample_id must reference either:
 *       * the immediately preceding record for that node_id, or
 *       * a well‑defined genesis ID.
 *
 * C2. Residual consistency:
 *   - vt_before for record i must equal vt_after for record i‑1,
 *     up to a small numeric tolerance.
 *
 * C3. Derived KER monotonicity guard:
 *   - If delta_vt > 0 (residual increases, more risk), then
 *       * e_i <= e_{i-1} + epsilon_E
 *       * r_i >= r_{i-1} - epsilon_R
 *     i.e., no unbounded E increase or R decrease when residual worsens.
 *
 * C4. Evidence binding:
 *   - evidence_hex must be unique per (node_id, sample_id) pair and
 *     must not be reused across records with differing residuals.
 *
 * C5. Chain completeness:
 *   - For each node_id, the sequence of prior_sample_id pointers forms
 *     a single, acyclic chain with no forks.
 *
 * These constraints are sufficient to prevent a single compromised sensor
 * from fabricating an isolated low‑R, high‑E workload if the residual
 * shows damage. Any attempt to do so will violate C2 or C3 and be flagged.
 */

struct IntegrityParams {
    double residual_tolerance;
    double epsilon_E;
    double epsilon_R;
};

struct IntegrityViolation {
    std::string node_id;
    std::string sample_id;
    std::string type;       // e.g., "C2_RESIDUAL_INCONSISTENT"
    std::string message;    // human‑readable explanation
};

using IntegrityReport = std::vector<IntegrityViolation>;

static bool nearly_equal(double a, double b, double tol)
{
    const double diff = std::abs(a - b);
    return diff <= tol;
}

/*
 * Security game:
 *
 *   Adversary A controls a single compromised producer for node n.
 *   A can:
 *     - Alter telemetry used to compute V_t and K,E,R for that node.
 *     - Propose new ProgressRecord rows for daily_progress for node n.
 *
 *   A cannot:
 *     - Rewrite existing rows (append‑only constraint).
 *     - Alter other nodes' records.
 *     - Change integrity parameters.
 *
 *   Goal of A:
 *     - Produce a record i such that:
 *         delta_vt(i) > 0 (real damage),
 *         e(i) >= e(i-1) + delta_E (false eco‑benefit),
 *         r(i) <= r(i-1) - delta_R (false risk reduction),
 *       while all integrity constraints C1..C5 still appear satisfied.
 *
 *   Defender's strategy:
 *     - Enforce C2 and C3 strictly on every commit.
 *     - Reject or quarantine any record violating these constraints.
 *
 *   Claim:
 *     - Under C2 and C3, A cannot achieve the goal without producing
 *       a detectable violation, because any positive delta_vt forces
 *       E and R to move in the direction of higher risk or lower
 *       eco‑benefit within the configured epsilons.
 */

/*
 * Check constraints C1‑C5 for a single node_id sequence.
 */
IntegrityReport check_node_chain(
    const std::string &node_id,
    const std::vector<ProgressRecord> &records,
    const IntegrityParams &params
)
{
    IntegrityReport report;

    if (records.empty()) {
        return report;
    }

    // C1: temporal ordering and prior_sample_id continuity.
    std::map<std::string, std::size_t> index_by_sample;
    index_by_sample.reserve(records.size());

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto &rec = records[i];

        if (rec.node_id != node_id) {
            IntegrityViolation v{
                node_id,
                rec.sample_id,
                "C1_NODE_MISMATCH",
                "Record node_id mismatch in chain check."
            };
            report.push_back(v);
        }

        // Lexicographic order: (yyyymmdd, sample_id) should be non‑decreasing.
        if (i > 0) {
            const auto &prev = records[i - 1];
            const std::pair<std::string, std::string> key_prev{
                prev.yyyymmdd, prev.sample_id};
            const std::pair<std::string, std::string> key_cur{
                rec.yyyymmdd, rec.sample_id};
            if (key_cur <= key_prev) {
                IntegrityViolation v{
                    node_id,
                    rec.sample_id,
                    "C1_NON_MONOTONIC_ORDER",
                    "Non‑monotonic (date, sample_id) ordering detected."
                };
                report.push_back(v);
            }
        }

        if (index_by_sample.find(rec.sample_id) != index_by_sample.end()) {
            IntegrityViolation v{
                node_id,
                rec.sample_id,
                "C1_DUPLICATE_SAMPLE_ID",
                "Duplicate sample_id in node chain."
            };
            report.push_back(v);
        }

        index_by_sample[rec.sample_id] = i;
    }

    // C5: prior_sample_id must form a single acyclic chain.
    std::set<std::string> visited;
    for (const auto &rec : records) {
        const std::string &psid = rec.prior_sample_id;
        if (psid.empty()) {
            // Genesis allowed; no constraint here.
            continue;
        }
        auto it = index_by_sample.find(psid);
        if (it == index_by_sample.end()) {
            IntegrityViolation v{
                node_id,
                rec.sample_id,
                "C5_MISSING_PRIOR_SAMPLE",
                "prior_sample_id does not refer to any known record."
            };
            report.push_back(v);
        }
        if (!visited.insert(psid).second) {
            IntegrityViolation v{
                node_id,
                rec.sample_id,
                "C5_CHAIN_FORK_OR_CYCLE",
                "prior_sample_id participates in fork or cycle."
            };
            report.push_back(v);
        }
    }

    // C2, C3, C4: residual consistency, KER monotonic guard, evidence binding.
    std::map<std::string, std::string> residual_signature_by_evidence;
    residual_signature_by_evidence.reserve(records.size());

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto &rec = records[i];

        // Evidence uniqueness and residual binding.
        const std::string sig = rec.node_id + "|" +
                                std::to_string(rec.vt_before) + "|" +
                                std::to_string(rec.vt_after);
        auto e_it = residual_signature_by_evidence.find(rec.evidence_hex);
        if (e_it == residual_signature_by_evidence.end()) {
            residual_signature_by_evidence[rec.evidence_hex] = sig;
        } else {
            if (e_it->second != sig) {
                IntegrityViolation v{
                    node_id,
                    rec.sample_id,
                    "C4_EVIDENCE_REUSE_MISMATCH",
                    "Evidence hex reused for different residuals."
                };
                report.push_back(v);
            }
        }

        if (i == 0) {
            continue; // No previous record to compare.
        }

        const auto &prev = records[i - 1];

        // C2: vt_before(i) ≈ vt_after(i‑1).
        if (!nearly_equal(rec.vt_before, prev.vt_after,
                          params.residual_tolerance)) {
            IntegrityViolation v{
                node_id,
                rec.sample_id,
                "C2_RESIDUAL_INCONSISTENT",
                "Residual vt_before does not match vt_after of previous record."
            };
            report.push_back(v);
        }

        // C3: KER monotonicity when residual worsens.
        const bool residual_worsened = rec.delta_vt > 0.0;
        if (residual_worsened) {
            // E must not jump upward beyond epsilon_E.
            if (rec.e > prev.e + params.epsilon_E) {
                IntegrityViolation v{
                    node_id,
                    rec.sample_id,
                    "C3_E_INCREASE_WITH_RESIDUAL_WORSENING",
                    "Eco‑impact E increased while residual risk delta_vt > 0."
                };
                report.push_back(v);
            }
            // R must not drop beyond epsilon_R.
            if (rec.r < prev.r - params.epsilon_R) {
                IntegrityViolation v{
                    node_id,
                    rec.sample_id,
                    "C3_R_DECREASE_WITH_RESIDUAL_WORSENING",
                    "Risk R decreased while residual risk delta_vt > 0."
                };
                report.push_back(v);
            }
        }
    }

    return report;
}

/*
 * Mitigation strategy:
 *
 *   - The CI pipeline or governance gate MUST run check_node_chain for
 *     every node_id prior to accepting new daily_progress rows.
 *   - If report is non‑empty, all offending rows are quarantined and
 *     must be re‑measured or rejected; they cannot be counted toward
 *     KER triads or lane promotion decisions.
 *
 * This converts the adversarial robustness question into a concrete
 * predicate set over immutable diagnostics, satisfying the non‑actuating
 * architecture requirement.
 */

int main(int argc, char **argv)
{
    // Example usage: in real deployment, records would be loaded from
    // SQLite or similar; here we construct a tiny sequence for sanity.
    (void)argc;
    (void)argv;

    IntegrityParams params;
    params.residual_tolerance = 1e-6;
    params.epsilon_E = 0.01;
    params.epsilon_R = 0.01;

    std::vector<ProgressRecord> records;

    ProgressRecord r0{
        "20260724", "S0001", "PHX-CANAL-NODE-WL-01",
        0.90, 0.88, 0.15,
        0.80, 0.80, 0.0,
        "0xPHX0001", "" // genesis
    };
    records.push_back(r0);

    ProgressRecord r1{
        "20260725", "S0002", "PHX-CANAL-NODE-WL-01",
        0.91, 0.89, 0.14,
        0.80, 0.78, -0.02,
        "0xPHX0002", "S0001"
    };
    records.push_back(r1);

    // Adversarial attempt: residual worsens, but E increases and R decreases.
    ProgressRecord r2{
        "20260726", "S0003", "PHX-CANAL-NODE-WL-01",
        0.92, 0.92, 0.10,
        0.78, 0.82, 0.04,
        "0xPHX0003", "S0002"
    };
    records.push_back(r2);

    IntegrityReport rep = check_node_chain("PHX-CANAL-NODE-WL-01", records, params);

    for (const auto &v : rep) {
        std::cerr << "[INTEGRITY] node=" << v.node_id
                  << " sample=" << v.sample_id
                  << " type=" << v.type
                  << " msg=" << v.message << "\n";
    }

    return rep.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}
