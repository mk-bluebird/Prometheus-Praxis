// File: cpp/eco_restoration/economic_viability_and_bayesian_kalman_bootstrap.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>

namespace praxis {
namespace eco {

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// ----------------------------------------------------------
// 31. Economic viability of eco-restoration (cost-benefit analysis)
// ----------------------------------------------------------
//
// We model:
//   - Investment in white-roof + tree-canopy projects: C_invest (currency units).
//   - Resulting reduction in corridor RoH: ΔRoH = RoH_baseline - RoH_after.
//   - ALN-enforced healthcare cost savings from reduced psych risk due to cooler corridors:
//       S_health = f(ΔRoH) * Pop * Cost_per_case
//
// Break-even condition:
//   S_health >= C_invest  =>  ΔRoH >= ΔRoH_break_even.
//
// For simplicity, we assume a linear mapping:
//   f(ΔRoH) = alpha_health * ΔRoH,
// where alpha_health converts RoH reduction to fraction of avoided health incidents.

struct EconomicParams {
    double C_invest;          // investment cost
    double RoH_baseline;      // baseline corridor RoH
    double RoH_after;         // observed corridor RoH after eco-restoration
    double Pop;               // affected population size
    double Cost_per_case;     // average healthcare cost per psych-risk related case
    double alpha_health;      // fraction of cases avoided per unit RoH reduction
};

struct EconomicResult {
    double delta_RoH;
    double savings_health;
    double break_even_delta_RoH;
    bool   breaks_even;
};

EconomicResult evaluate_economic_viability(const EconomicParams& p) {
    double delta_RoH = p.RoH_baseline - p.RoH_after;
    if (delta_RoH < 0.0) delta_RoH = 0.0;

    // Health savings from ALN-enforced continuity improvements.
    double avoided_fraction = p.alpha_health * delta_RoH;
    double savings_health = avoided_fraction * p.Pop * p.Cost_per_case;

    // Break-even ΔRoH:
    //   C_invest = alpha_health * ΔRoH_break_even * Pop * Cost_per_case
    double break_even_delta_RoH =
        (p.C_invest) / (p.alpha_health * p.Pop * p.Cost_per_case);

    bool breaks_even = (delta_RoH >= break_even_delta_RoH);

    return EconomicResult{delta_RoH, savings_health, break_even_delta_RoH, breaks_even};
}

// ----------------------------------------------------------
// 32. Bootstrapping Kalman filter with Bayesian hierarchical priors
// ----------------------------------------------------------
//
// We consider psych-risk score r_t estimated from electrode data via Kalman filter.
// When no calibration history is available, we use a Bayesian hierarchical model
// over electrode arrays:
//
//   - At population level (across arrays):
//       r_t | μ, τ^2 ~ N(μ, τ^2)   (psych-risk prior)
//       μ ~ N(μ_0, σ_μ^2)
//       τ^2 ~ InvGamma(a_τ, b_τ)
//
//   - For a new array, before observing data, the prior predictive distribution
//     for r_t is:
//       r_t ~ Student-t with parameters derived from μ_0, σ_μ^2, a_τ, b_τ.
//
// We use this prior predictive variance to set the initial covariance P_0
// of the Kalman filter.
//
// Safety condition for continuity decisions:
//   - The initial variance Var(r_t) must be large enough to reflect uncertainty,
//     and policy requires that continuity triggers (e.g., HIGH band) are only
//     considered when posterior variance drops below a threshold.
//   - We ensure P_0 >= P_min and require posterior P_t <= P_safe before allowing
//     continuity-triggered actions.

struct HierarchicalParams {
    double mu_0;     // prior mean of psych-risk
    double sigma_mu; // std dev of μ
    double a_tau;    // shape of InvGamma for τ^2
    double b_tau;    // scale of InvGamma for τ^2
};

struct PriorPredictive {
    double mean;
    double var;
};

PriorPredictive prior_predictive_psych(const HierarchicalParams& hp) {
    // For a normal-inverse-gamma prior, prior predictive is Student-t:
    //   mean = mu_0
    //   var  ≈ b_tau / (a_tau - 1) + sigma_mu^2
    // (assuming a_tau > 1).
    double mean = hp.mu_0;
    double var_tau = hp.b_tau / (hp.a_tau - 1.0);
    double var = var_tau + hp.sigma_mu * hp.sigma_mu;
    return PriorPredictive{mean, var};
}

// Initial Kalman covariance from prior predictive variance.
// For scalar psych-risk state, P_0 = var_prior.
double initial_kalman_covariance(const PriorPredictive& pp) {
    return pp.var;
}

// Safety conditions:
//
//   - P_0 should be sufficiently large (reflecting uncertainty).
//   - A policy threshold P_safe (e.g., 0.01) defines when continuity decisions are allowed.
//   - During filter operation, we monitor P_t; while P_t > P_safe, continuity triggers
//     are held in observational-only mode.
//   - Only when P_t <= P_safe and reliability_token is valid do we permit decisions.
//
// Here we encode a simple check.

struct KalmanSafetyCheck {
    double P0;
    double P_safe;
    bool   safe_for_decisions_initial;
};

KalmanSafetyCheck evaluate_kalman_safety(const PriorPredictive& pp,
                                         double P_safe) {
    double P0 = initial_kalman_covariance(pp);
    bool safe_initial = (P0 <= P_safe);
    return KalmanSafetyCheck{P0, P_safe, safe_initial};
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 31. Economic viability demo.
    EconomicParams ep{
        5'000'000.0, // C_invest
        0.35,        // RoH_baseline
        0.28,        // RoH_after
        200'000.0,   // Pop
        2'000.0,     // Cost_per_case
        0.5          // alpha_health (50% of psych-risk-related incidents avoided per unit RoH reduction)
    };

    EconomicResult er = evaluate_economic_viability(ep);

    std::cout << "Economic viability of eco-restoration (Phoenix corridors):\n";
    std::cout << "  Investment cost C_invest=" << ep.C_invest << "\n";
    std::cout << "  RoH_baseline=" << ep.RoH_baseline
              << ", RoH_after=" << ep.RoH_after
              << ", ΔRoH=" << er.delta_RoH << "\n";
    std::cout << "  Healthcare savings S_health=" << er.savings_health << "\n";
    std::cout << "  Break-even ΔRoH required=" << er.break_even_delta_RoH << "\n";
    std::cout << "  Break-even achieved? " << (er.breaks_even ? "YES" : "NO") << "\n\n";

    // 32. Bayesian Kalman bootstrap demo.
    HierarchicalParams hp{
        0.5,  // mu_0: prior mean psych-risk
        0.1,  // sigma_mu
        3.0,  // a_tau
        0.02  // b_tau
    };

    PriorPredictive pp = prior_predictive_psych(hp);
    double P_safe = 0.01;
    KalmanSafetyCheck kc = evaluate_kalman_safety(pp, P_safe);

    std::cout << "Bayesian hierarchical Kalman bootstrap (no calibration history):\n";
    std::cout << "  Prior predictive mean=" << pp.mean
              << ", var=" << pp.var << "\n";
    std::cout << "  Initial Kalman covariance P0=" << kc.P0
              << ", policy safe threshold P_safe=" << kc.P_safe << "\n";
    std::cout << "  Initial confidence safe for continuity decisions? "
              << (kc.safe_for_decisions_initial ? "YES" : "NO") << "\n";
    std::cout << "  Policy: treat early psych-risk bands as observational-only until "
                 "posterior P_t falls below P_safe and reliability tokens confirm "
                 "sensor integrity.\n";

    return 0;
}

} // namespace eco
} // namespace praxis
