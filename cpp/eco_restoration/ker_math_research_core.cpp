// File: cpp/eco_restoration/ker_math_research_core.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <random>
#include <algorithm>

namespace eco_restoration {

// KER triad with scalar consistency and bounds invariants.
struct KerProfile {
    double k;   // knowledge-factor 0..1
    double e;   // eco-impact 0..1
    double r;   // risk-of-harm 0..1
    double s;   // scalar corridor = k * e - r

    KerProfile(double k_, double e_, double r_)
        : k(k_), e(e_), r(r_), s(k_ * e_ - r_) {}

    void recompute() {
        s = k * e - r;
    }

    bool bounds_ok() const {
        return k >= 0.0 && k <= 1.0 &&
               e >= 0.0 && e <= 1.0 &&
               r >= 0.0 && r <= 1.0;
    }

    bool scalar_consistency_ok(double tol = 1e-12) const {
        double expected = k * e - r;
        return std::fabs(expected - s) < tol;
    }
};

// Lanes for governance.
enum class Lane {
    RESEARCH,
    EXPPROD,
    PROD
};

static std::string lane_to_string(Lane lane) {
    switch (lane) {
        case Lane::RESEARCH: return "RESEARCH";
        case Lane::EXPPROD:  return "EXPPROD";
        case Lane::PROD:     return "PROD";
    }
    return "RESEARCH";
}

// Module governance invariants modeled as mathematical predicates.
struct ModuleGovernance {
    std::string relpath;
    std::string module_role;   // "ANALYTIC", "PURE_GOVERNANCE", "ACTUATION"
    bool non_actuating;
    bool neuro_flag;
    bool citizen_ready;
    Lane lane;
    KerProfile ker;

    ModuleGovernance(std::string path,
                     std::string role,
                     bool non_act,
                     bool neuro,
                     bool citizen,
                     Lane lane_,
                     KerProfile ker_)
        : relpath(std::move(path)),
          module_role(std::move(role)),
          non_actuating(non_act),
          neuro_flag(neuro),
          citizen_ready(citizen),
          lane(lane_),
          ker(ker_) {}

    bool ker_bounds_ok(std::string &reason) const {
        if (!ker.bounds_ok()) {
            reason = "KerBounds violated: K,E,R must be in [0,1]";
            return false;
        }
        if (!ker.scalar_consistency_ok()) {
            reason = "KerScalarConsistency violated: s != k*e - r";
            return false;
        }
        return true;
    }

    bool ker_positive_for_non_research_ok(std::string &reason) const {
        if (lane != Lane::RESEARCH && ker.s <= 0.0) {
            reason = "KerPositiveForNonResearch violated: lane!=RESEARCH requires s>0";
            return false;
        }
        return true;
    }

    bool non_actuating_role_consistency_ok(std::string &reason) const {
        if (non_actuating) {
            if (module_role != "ANALYTIC" && module_role != "PURE_GOVERNANCE") {
                reason = "NonActuatingRoleConsistency violated: non_actuating must be ANALYTIC or PURE_GOVERNANCE";
                return false;
            }
        }
        return true;
    }

    bool prod_lane_governance_ok(std::string &reason) const {
        if (lane != Lane::PROD) {
            return true;
        }
        if (!(ker.s > 0.2 && ker.e >= 0.7 && ker.r <= 0.5 && citizen_ready)) {
            reason = "ProdLaneGovernance violated: PROD requires s>0.2, e>=0.7, r<=0.5, citizen_ready=true";
            return false;
        }
        return true;
    }

    bool high_risk_governance_ok(std::string &reason) const {
        if (ker.r >= 0.7) {
            if (!(neuro_flag && !citizen_ready && lane == Lane::RESEARCH)) {
                reason = "HighRiskGovernance violated: r>=0.7 must imply neuro_flag=true, citizen_ready=false, lane=RESEARCH";
                return false;
            }
        }
        return true;
    }

    bool all_invariants_ok(std::string &reason) const {
        if (!ker_bounds_ok(reason)) return false;
        if (!ker_positive_for_non_research_ok(reason)) return false;
        if (!non_actuating_role_consistency_ok(reason)) return false;
        if (!prod_lane_governance_ok(reason)) return false;
        if (!high_risk_governance_ok(reason)) return false;
        return true;
    }
};

// Lyapunov corridor model for ΔVt, parameterized for research experiments.
struct LyapunovCorridor {
    double alpha;              // eco-efficiency sensitivity
    double beta;               // ΔVt sensitivity to energy
    double delta_v_max;        // maximum allowed ΔVt per step

    LyapunovCorridor(double a, double b, double dv_max)
        : alpha(a), beta(b), delta_v_max(dv_max) {}

    // Compute eco-weighted energy and ΔVt, enforcing corridor bounds.
    double eco_weighted_energy(double energy_req_j, double eco_efficiency) const {
        double eff = std::clamp(eco_efficiency, 0.0, 1.0);
        return energy_req_j * (1.0 + alpha * (1.0 - eff));
    }

    double delta_v_t_step(double energy_req_j, double eco_efficiency) const {
        double e_eco = eco_weighted_energy(energy_req_j, eco_efficiency);
        double dvt = beta * e_eco;
        if (dvt < 0.0) dvt = 0.0;
        if (dvt > delta_v_max) dvt = delta_v_max;
        return dvt;
    }

    bool corridor_ok(double dvt) const {
        return dvt >= 0.0 && dvt <= delta_v_max;
    }
};

// Sensitivity analysis on KER scalar for lane decisions.
struct KerSensitivityResult {
    double k;
    double e;
    double r;
    double s;
    Lane lane;
};

std::vector<KerSensitivityResult> sweep_ker_for_lane(Lane lane,
                                                     double fixed_r,
                                                     double k_min,
                                                     double k_max,
                                                     double e_min,
                                                     double e_max,
                                                     std::size_t steps_k,
                                                     std::size_t steps_e) {
    std::vector<KerSensitivityResult> results;
    if (steps_k == 0 || steps_e == 0) {
        return results;
    }
    double dk = (k_max - k_min) / static_cast<double>(steps_k);
    double de = (e_max - e_min) / static_cast<double>(steps_e);

    for (std::size_t i = 0; i <= steps_k; ++i) {
        double k = k_min + i * dk;
        for (std::size_t j = 0; j <= steps_e; ++j) {
            double e = e_min + j * de;
            KerProfile ker(k, e, fixed_r);
            ker.recompute();
            KerSensitivityResult r{};
            r.k = k;
            r.e = e;
            r.r = fixed_r;
            r.s = ker.s;
            r.lane = lane;
            results.push_back(r);
        }
    }
    return results;
}

// Simple probabilistic risk model: ker_r as mean of a Beta distribution, Monte Carlo corridor check.
struct ProbabilisticKerRisk {
    double alpha;  // shape parameters for Beta distribution (risk)
    double beta;   // shape parameters for Beta distribution
    double threshold;          // risk threshold
    double max_violation_prob; // acceptable probability of exceeding threshold

    ProbabilisticKerRisk(double a, double b, double t, double max_p)
        : alpha(a), beta(b), threshold(t), max_violation_prob(max_p) {}

    // Sample from Beta(alpha, beta) using inverse CDF via rejection sampling.
    double sample_beta(std::mt19937_64 &rng) const {
        std::gamma_distribution<double> dist_a(alpha, 1.0);
        std::gamma_distribution<double> dist_b(beta, 1.0);
        double xa = dist_a(rng);
        double xb = dist_b(rng);
        if (xa <= 0.0 && xb <= 0.0) {
            return 0.0;
        }
        return xa / (xa + xb);
    }

    // Estimate probability that ker_r exceeds threshold.
    double estimate_violation_probability(std::size_t samples, std::mt19937_64 &rng) const {
        if (samples == 0) return 0.0;
        std::size_t violations = 0;
        for (std::size_t i = 0; i < samples; ++i) {
            double r_sample = sample_beta(rng);
            if (r_sample > threshold) {
                ++violations;
            }
        }
        return static_cast<double>(violations) / static_cast<double>(samples);
    }

    bool corridor_ok(std::size_t samples, std::mt19937_64 &rng, double &estimated_prob) const {
        estimated_prob = estimate_violation_probability(samples, rng);
        return estimated_prob <= max_violation_prob;
    }
};

// Simple research harness printing results for KER and Lyapunov sensitivity.
int main() {
    // Baseline modules inspired by eco_synapse bridge and cyboquatic simulator.
    ModuleGovernance cpp_bridge(
        "cpp/eco_restoration/eco_synapse_cpp_bridge.cpp",
        "ANALYTIC",
        true,
        false,
        true,
        Lane::EXPPROD,
        KerProfile(0.9, 0.8, 0.2)
    );
    cpp_bridge.ker.recompute();

    ModuleGovernance cybo_sim(
        "cpp/simulation/cyboquatic_workload_simulator.cpp",
        "ANALYTIC",
        true,
        false,
        true,
        Lane::EXPPROD,
        KerProfile(0.9, 0.85, 0.35)
    );
    cybo_sim.ker.recompute();

    std::string reason;
    std::cout << "=== Module invariant check ===\n";
    std::cout << "CPP eco_synapse bridge: ";
    if (cpp_bridge.all_invariants_ok(reason)) {
        std::cout << "OK (s=" << cpp_bridge.ker.s << ", lane=" << lane_to_string(cpp_bridge.lane) << ")\n";
    } else {
        std::cout << "VIOLATION: " << reason << "\n";
    }

    std::cout << "Cyboquatic simulator: ";
    if (cybo_sim.all_invariants_ok(reason)) {
        std::cout << "OK (s=" << cybo_sim.ker.s << ", lane=" << lane_to_string(cybo_sim.lane) << ")\n";
    } else {
        std::cout << "VIOLATION: " << reason << "\n";
    }

    // Lyapunov corridor exploration.
    LyapunovCorridor corridor(0.5, 1e-6, 0.05);
    std::cout << "\n=== Lyapunov corridor sensitivity (ΔVt vs eco_efficiency) ===\n";
    double energy_req_j = 1.0e6; // example energy per step in Joules
    for (double eff = 0.0; eff <= 1.0; eff += 0.2) {
        double dvt = corridor.delta_v_t_step(energy_req_j, eff);
        std::cout << "eco_efficiency=" << eff
                  << " -> ΔVt=" << dvt
                  << " (corridor_ok=" << (corridor.corridor_ok(dvt) ? "true" : "false") << ")\n";
    }

    // KER sensitivity sweep for PROD lane, fixed risk r=0.3.
    std::cout << "\n=== KER sensitivity sweep for PROD lane (r=0.3) ===\n";
    auto sweep = sweep_ker_for_lane(Lane::PROD, 0.3, 0.5, 1.0, 0.5, 1.0, 5, 5);
    for (const auto &res : sweep) {
        bool prod_ok = (res.s > 0.2 && res.e >= 0.7 && res.r <= 0.5);
        std::cout << "k=" << res.k
                  << ", e=" << res.e
                  << ", r=" << res.r
                  << ", s=" << res.s
                  << " -> PROD corridor " << (prod_ok ? "OK" : "NO") << "\n";
    }

    // Probabilistic risk corridor example using Beta distribution.
    std::cout << "\n=== Probabilistic risk corridor (Beta distribution) ===\n";
    ProbabilisticKerRisk prob_risk(2.0, 5.0, 0.7, 0.05); // mean risk < threshold with small violation prob
    std::mt19937_64 rng(123456789ULL);
    double estimated_prob = 0.0;
    bool ok = prob_risk.corridor_ok(10000, rng, estimated_prob);
    std::cout << "Estimated P(ker_r > " << prob_risk.threshold << ") = "
              << estimated_prob << " -> corridor_ok=" << (ok ? "true" : "false") << "\n";

    return 0;
}

} // namespace eco_restoration
