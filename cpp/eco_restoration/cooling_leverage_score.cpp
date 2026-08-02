// File: cpp/eco_restoration/cooling_leverage_score.cpp

#include <vector>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <string>

/**
 * Cooling-leverage score L_h for hex h with uncertainty propagation.
 *
 * Assume the calibrated offset model for ΔT_h is:
 *   ΔT_h = α V_h + β B_h + γ W_h + δ
 *
 * where:
 *   V_h : vegetation / tree canopy metric (higher V_h usually cools, α < 0).
 *   B_h : built / roof metric (higher B_h often warms, β > 0, but cool roofs reduce β).
 *   W_h : water / surface wetness metric (higher W_h cools, γ < 0).
 *
 * For planning, we define a "cooling leverage" score that aggregates potential
 * cooling from tree, roof, and water interventions and accounts for uncertainty
 * in α, β, γ (estimated via regression and carrying standard errors).
 *
 * Let:
 *   α_hat, β_hat, γ_hat  : mean parameter estimates.
 *   σ_α, σ_β, σ_γ       : standard deviations (uncertainties) of α, β, γ.
 *   V_h^*, B_h^*, W_h^* : achievable intervention increments (e.g., ΔNDVI from tree planting,
 *                         Δroof_albedo from cool roofs, Δwater index from water features).
 *
 * Nominal cooling potential for hex h:
 *   C_h = α_hat V_h^* + β_hat B_h^* + γ_hat W_h^*
 *
 * Assuming α, β, γ are independent, variance of C_h is:
 *   Var(C_h) = (V_h^*)^2 σ_α^2 + (B_h^*)^2 σ_β^2 + (W_h^*)^2 σ_γ^2
 *
 * A risk-adjusted cooling leverage score can be:
 *   L_h = C_h - k * sqrt(Var(C_h))
 *
 * where k is a risk-aversion factor (e.g., k = 1 for 1σ, k = 1.96 for ~95% CI).
 * For ranking, higher L_h implies higher priority under uncertainty.
 */

struct CoolingParameters {
    double alpha_hat;
    double beta_hat;
    double gamma_hat;
    double sigma_alpha;
    double sigma_beta;
    double sigma_gamma;
};

struct InterventionIncrements {
    double delta_tree;   // V_h^* : expected vegetation/tree canopy gain (e.g., ΔNDVI or canopy fraction)
    double delta_roof;   // B_h^* : expected cool-roof / roof-albedo gain (negative ΔB if cooling)
    double delta_water;  // W_h^* : expected water-feature / wetness gain
};

struct CoolingLeverageResult {
    double C_nominal; // nominal cooling potential
    double variance;  // propagated variance of C_h
    double stddev;    // sqrt(variance)
    double L_score;   // risk-adjusted cooling leverage
};

CoolingLeverageResult compute_cooling_leverage(const CoolingParameters& params,
                                               const InterventionIncrements& inc,
                                               double risk_factor_k) {
    // Nominal cooling potential (units: temperature change).
    double C_nominal = params.alpha_hat * inc.delta_tree
                     + params.beta_hat  * inc.delta_roof
                     + params.gamma_hat * inc.delta_water;

    // Variance propagation assuming independence of α, β, γ.[81][91][84]
    double var_C = inc.delta_tree * inc.delta_tree * params.sigma_alpha * params.sigma_alpha
                 + inc.delta_roof * inc.delta_roof * params.sigma_beta * params.sigma_beta
                 + inc.delta_water * inc.delta_water * params.sigma_gamma * params.sigma_gamma;

    if (var_C < 0.0) {
        var_C = 0.0;
    }

    double std_C = std::sqrt(var_C);

    // Risk-adjusted leverage score: penalize nominal cooling by uncertainty.
    double L = C_nominal - risk_factor_k * std_C;

    CoolingLeverageResult res;
    res.C_nominal = C_nominal;
    res.variance = var_C;
    res.stddev = std_C;
    res.L_score = L;
    return res;
}

struct HexCoolingPriority {
    std::string hex_id;
    InterventionIncrements inc;
    CoolingLeverageResult leverage;
};

std::vector<HexCoolingPriority> rank_hexes_by_leverage(
        const std::vector<std::string>& hex_ids,
        const std::vector<InterventionIncrements>& increments,
        const CoolingParameters& params,
        double risk_factor_k) {
    if (hex_ids.size() != increments.size()) {
        throw std::invalid_argument("hex_ids and increments must have same length.");
    }

    std::vector<HexCoolingPriority> priorities;
    priorities.reserve(hex_ids.size());

    for (std::size_t i = 0; i < hex_ids.size(); ++i) {
        HexCoolingPriority p;
        p.hex_id = hex_ids[i];
        p.inc = increments[i];
        p.leverage = compute_cooling_leverage(params, increments[i], risk_factor_k);
        priorities.push_back(p);
    }

    // Sort descending by risk-adjusted leverage score.
    std::sort(priorities.begin(), priorities.end(),
              [](const HexCoolingPriority& a, const HexCoolingPriority& b) {
                  return a.leverage.L_score > b.leverage.L_score;
              });

    return priorities;
}

// Example usage: compare hex priorities under propagated α, β, γ uncertainty.
int main() {
    CoolingParameters params;
    params.alpha_hat = -8.0;   // tree cooling coefficient (°C per unit ΔNDVI, approximate)
    params.beta_hat  = 3.0;    // roof effect (°C per unit built/roof index; cool roofs reduce β)[93][87][90]
    params.gamma_hat = -5.0;   // water cooling coefficient
    params.sigma_alpha = 0.8;  // uncertainties from regression (standard errors)
    params.sigma_beta  = 0.6;
    params.sigma_gamma = 0.7;

    std::vector<std::string> hex_ids = {"hex_10_20", "hex_11_20", "hex_12_20"};
    std::vector<InterventionIncrements> incs(3);

    // Hex 10_20: strong tree potential, modest roof, low water.
    incs[0].delta_tree  = 0.15;
    incs[0].delta_roof  = -0.05;  // cool roofs reduce warming contribution
    incs[0].delta_water = 0.02;

    // Hex 11_20: modest tree, strong roof retrofit, some water.
    incs[1].delta_tree  = 0.08;
    incs[1].delta_roof  = -0.12;
    incs[1].delta_water = 0.04;

    // Hex 12_20: limited tree (space constrained), mild roof, strong water feature potential.
    incs[2].delta_tree  = 0.04;
    incs[2].delta_roof  = -0.06;
    incs[2].delta_water = 0.10;

    double risk_factor_k = 1.96; // approximate 95% confidence band for risk adjustment

    auto ranked = rank_hexes_by_leverage(hex_ids, incs, params, risk_factor_k);

    std::cout << "Risk-adjusted cooling leverage ranking:\n";
    for (const auto& p : ranked) {
        std::cout << "Hex " << p.hex_id
                  << " | C_nominal=" << p.leverage.C_nominal
                  << " | sigma=" << p.leverage.stddev
                  << " | L_score=" << p.leverage.L_score << "\n";
    }

    return 0;
}
