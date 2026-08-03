// File: cpp/tools/cross_shard_two_phase_commit_and_psychometric_power.cpp
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <iomanip>
#include <cmath>

namespace praxis {
namespace tools {

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// -----------------------------
// Cross-shard two-phase commit (KER governance)
// -----------------------------

namespace eco {

enum class ShardVote {
    PREPARED,
    ABORT
};

enum class GlobalDecision {
    COMMIT,
    ABORT
};

struct KerUpdate {
    std::string shard_id;
    std::string module_id;
    double k_new;
    double e_new;
    double r_new;
};

struct ShardPrepareResult {
    std::string shard_id;
    ShardVote vote;
    std::string reason;
};

struct TwoPhaseCommitResult {
    GlobalDecision decision;
    std::vector<ShardPrepareResult> shard_results;
};

struct ShardGovernanceConfig {
    double s_min_non_research;
    double s_max;
};

double ker_scalar(double k, double e, double r) {
    k = clamp01(k);
    e = clamp01(e);
    r = clamp01(r);
    return k * e - r;
}

ShardPrepareResult shard_prepare(const KerUpdate& upd,
                                 const ShardGovernanceConfig& cfg) {
    ShardPrepareResult res{};
    res.shard_id = upd.shard_id;
    double s = ker_scalar(upd.k_new, upd.e_new, upd.r_new);

    if (s < cfg.s_min_non_research) {
        res.vote = ShardVote::ABORT;
        res.reason = "KER scalar below s_min_non_research";
        return res;
    }
    if (s > cfg.s_max) {
        res.vote = ShardVote::ABORT;
        res.reason = "KER scalar above s_max corridor";
        return res;
    }

    res.vote = ShardVote::PREPARED;
    res.reason = "KER update satisfies shard governance constraints";
    return res;
}

TwoPhaseCommitResult run_two_phase_commit(
        const std::vector<KerUpdate>& updates,
        const std::unordered_map<std::string, ShardGovernanceConfig>& configs) {

    TwoPhaseCommitResult result{};
    result.decision = GlobalDecision::ABORT;

    bool all_prepared = true;
    for (const auto& upd : updates) {
        auto it = configs.find(upd.shard_id);
        ShardPrepareResult res{};
        if (it == configs.end()) {
            res.shard_id = upd.shard_id;
            res.vote = ShardVote::ABORT;
            res.reason = "No governance config for shard";
            all_prepared = false;
        } else {
            res = shard_prepare(upd, it->second);
            if (res.vote == ShardVote::ABORT) {
                all_prepared = false;
            }
        }
        result.shard_results.push_back(res);
    }

    result.decision = all_prepared ? GlobalDecision::COMMIT : GlobalDecision::ABORT;
    return result;
}

void emit_two_phase_sql(const std::vector<KerUpdate>& updates,
                        const TwoPhaseCommitResult& res) {
    std::cout << "-- Phase 1: PREPARE KER updates\n";
    for (const auto& upd : updates) {
        std::cout << "/* shard " << upd.shard_id << " */ "
                  << "INSERT INTO ker_update_staging "
                  << "(shard_id, module_id, k_new, e_new, r_new) VALUES ('"
                  << upd.shard_id << "', '"
                  << upd.module_id << "', "
                  << upd.k_new << ", "
                  << upd.e_new << ", "
                  << upd.r_new << ");\n";
    }

    std::cout << "\n-- Shard votes\n";
    for (const auto& sr : res.shard_results) {
        std::cout << "/* shard " << sr.shard_id << " vote="
                  << (sr.vote == ShardVote::PREPARED ? "PREPARED" : "ABORT")
                  << " reason=" << sr.reason << " */\n";
    }

    std::cout << "\n-- Phase 2: GLOBAL "
              << (res.decision == GlobalDecision::COMMIT ? "COMMIT" : "ABORT")
              << "\n";

    if (res.decision == GlobalDecision::COMMIT) {
        for (const auto& upd : updates) {
            std::cout << "/* shard " << upd.shard_id << " */ "
                      << "UPDATE module_ker_profile SET "
                      << "ker_k = " << upd.k_new << ", "
                      << "ker_e = " << upd.e_new << ", "
                      << "ker_r = " << upd.r_new << ", "
                      << "ker_s = " << ker_scalar(upd.k_new, upd.e_new, upd.r_new)
                      << " WHERE module_id = '" << upd.module_id << "';\n";
        }
        std::cout << "DELETE FROM ker_update_staging;\n";
    } else {
        std::cout << "DELETE FROM ker_update_staging;\n";
        std::cout << "-- All staged updates rolled back due to ABORT decision.\n";
    }
}

} // namespace eco

// -----------------------------
// Labor / eco cross-shard RoH corridor 2PC
// -----------------------------

enum class Vote {
    COMMIT,
    ABORT
};

struct ShardState {
    double RoH_corridor;
    bool labor_safe;
    bool eco_safe;
};

struct ProposedUpdate {
    double delta_RoH_lab;
    double delta_RoH_eco;
    bool labor_change_safe;
    bool eco_change_safe;
};

bool invariants_lab_hold(const ShardState& s, const ProposedUpdate& u) {
    double RoH_new = s.RoH_corridor + u.delta_RoH_lab;
    bool roh_ok = RoH_new <= 0.30;
    bool labor_ok = s.labor_safe && u.labor_change_safe;
    return roh_ok && labor_ok;
}

bool invariants_eco_hold(const ShardState& s, const ProposedUpdate& u) {
    double RoH_new = s.RoH_corridor + u.delta_RoH_eco;
    bool roh_ok = RoH_new <= 0.30;
    bool eco_ok = s.eco_safe && u.eco_change_safe;
    return roh_ok && eco_ok;
}

Vote prepare_lab(const ShardState& s, const ProposedUpdate& u) {
    return invariants_lab_hold(s, u) ? Vote::COMMIT : Vote::ABORT;
}

Vote prepare_eco(const ShardState& s, const ProposedUpdate& u) {
    return invariants_eco_hold(s, u) ? Vote::COMMIT : Vote::ABORT;
}

bool coordinator_two_phase_commit(const ShardState& lab_state,
                                  const ShardState& eco_state,
                                  const ProposedUpdate& u_lab,
                                  const ProposedUpdate& u_eco,
                                  ShardState& lab_state_out,
                                  ShardState& eco_state_out) {
    Vote v_lab = prepare_lab(lab_state, u_lab);
    Vote v_eco = prepare_eco(eco_state, u_eco);

    if (v_lab == Vote::COMMIT && v_eco == Vote::COMMIT) {
        lab_state_out = lab_state;
        eco_state_out = eco_state;
        lab_state_out.RoH_corridor += u_lab.delta_RoH_lab;
        eco_state_out.RoH_corridor += u_eco.delta_RoH_eco;
        lab_state_out.labor_safe = lab_state.labor_safe && u_lab.labor_change_safe;
        eco_state_out.eco_safe   = eco_state.eco_safe   && u_eco.eco_change_safe;
        return true;
    } else {
        lab_state_out = lab_state;
        eco_state_out = eco_state;
        return false;
    }
}

// -----------------------------
// Psychometric power analysis
// -----------------------------

struct PowerParams {
    double alpha;
    double power;
    double rho_target;
};

double erfinv_series(double x) {
    double pi_over_4 = 3.14159265358979323846 / 4.0;
    double a = pi_over_4 * (x);
    double x3 = x * x * x;
    double x5 = x3 * x * x;
    return a + (pi_over_4 * pi_over_4 * a * x3) / 3.0 +
           (7.0 * std::pow(pi_over_4, 3) * a * x5) / 30.0;
}

double z_quantile(double p) {
    double t = 2.0 * p - 1.0;
    double inv_erf = erfinv_series(t);
    return std::sqrt(2.0) * inv_erf;
}

double required_sample_size_for_correlation(const PowerParams& pp) {
    double Z_alpha = z_quantile(1.0 - pp.alpha / 2.0);
    double Z_beta  = z_quantile(pp.power);
    double num = (Z_alpha + Z_beta) * (Z_alpha + Z_beta);

    double fisher_z = 0.5 * std::log((1.0 + pp.rho_target) / (1.0 - pp.rho_target));
    double denom = fisher_z * fisher_z;
    if (denom <= 0.0) return 0.0;

    return num / denom;
}

int main() {
    using namespace eco;
    std::cout << std::fixed << std::setprecision(4);

    std::unordered_map<std::string, ShardGovernanceConfig> configs;
    configs["shard_A"] = {0.05, 0.9};
    configs["shard_B"] = {0.05, 0.9};

    std::vector<KerUpdate> updates = {
        {"shard_A", "module_A1", 0.8, 0.75, 0.3},
        {"shard_B", "module_B2", 0.7, 0.65, 0.4}
    };

    TwoPhaseCommitResult res = run_two_phase_commit(updates, configs);

    std::cout << "Cross-shard two-phase commit decision: "
              << (res.decision == GlobalDecision::COMMIT ? "COMMIT" : "ABORT") << "\n\n";
    emit_two_phase_sql(updates, res);
    std::cout << "\n";

    ShardState lab_state{0.28, true, false};
    ShardState eco_state{0.28, false, true};
    ProposedUpdate u_lab{0.01, 0.0, true, false};
    ProposedUpdate u_eco{0.01, 0.0, false, true};
    ShardState lab_out{}, eco_out{};
    bool committed = coordinator_two_phase_commit(lab_state, eco_state,
                                                  u_lab, u_eco,
                                                  lab_out, eco_out);

    std::cout << "Labor/Eco RoH two-phase commit:\n";
    std::cout << "  Commit decision: " << (committed ? "COMMIT" : "ABORT") << "\n";
    std::cout << "  Lab RoH after: " << lab_out.RoH_corridor
              << ", Eco RoH after: " << eco_out.RoH_corridor << "\n";
    std::cout << "  RoH invariant preserved (<=0.30)? "
              << ((lab_out.RoH_corridor <= 0.30 && eco_out.RoH_corridor <= 0.30)
                  ? "YES" : "NO") << "\n\n";

    PowerParams pp{0.05, 0.80, 0.30};
    double N_required = required_sample_size_for_correlation(pp);

    std::cout << "Psychometric validation power analysis (NASA-TLX vs psych-risk):\n";
    std::cout << "  Target correlation rho=" << pp.rho_target
              << ", alpha=" << pp.alpha
              << ", power=" << pp.power << "\n";
    std::cout << "  Approximate sample size required N≈" << std::ceil(N_required) << "\n";

    return 0;
}

} // namespace tools
} // namespace praxis
