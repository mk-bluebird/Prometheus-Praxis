// File: cpp/eco_restoration/equity_weighted_cooling_benefit.cpp

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>

/**
 * Equity-weighted expected cooling benefit per dollar for tree interventions.
 *
 * Tree-priority score with equity weighting:
 *
 *   P_h^{equity} =
 *       UHI_h ⋅ |α| ⋅ (V_max - V_h) ⋅ f_feasible(h) ⋅ exp(-λ ⋅ SVI_h)
 *
 * where:
 *   UHI_h      : current hex UHI intensity.
 *   α          : vegetation cooling coefficient (°C per unit ΔV).
 *   V_max      : maximum feasible vegetation index (or canopy fraction).
 *   V_h        : current vegetation index.
 *   f_feasible : feasibility indicator (0–1) for tree intervention.
 *   SVI_h      : CDC Social Vulnerability Index for the hex.[90]
 *   λ          : equity parameter; larger λ down-weights low-vulnerability
 *                (low SVI) areas and relatively up-weights high-SVI areas
 *                when calibrated appropriately.
 *
 * Expected cooling per dollar:
 *
 *   B_h = P_h^{equity} / cost_eff_h
 *
 * where cost_eff_h includes non-monetary weights as previously defined.
 *
 * Calibration of λ from community engagement:
 *
 *  - Suppose we have community-derived priority weights W_comm,h for hexes
 *    (e.g., from surveys, participatory mapping, or DAO votes) indicating
 *    desired relative priority for cooling.
 *
 *  - We choose λ to minimize:
 *
 *      L(λ) = Σ_h [ P_h^{equity}(λ) / Z(λ) - W_comm,h ]^2
 *
 *    where Z(λ) is a normalization factor, e.g., Σ_h P_h^{equity}(λ), so that
 *    P_h^{equity}/Z(λ) forms a probability distribution over hexes.
 *
 *  - We can perform a simple one-dimensional search over λ ≥ 0 to minimize L(λ),
 *    or use gradient-based optimization in λ.
 */

struct HexTreeData {
    std::string hex_id;
    double UHI;
    double alpha;
    double V_max;
    double V_current;
    double feasible; // f_feasible(h) ∈ [0,1]
    double SVI;
    double cost_eff; // effective cost including non-monetary weights
    double community_weight; // W_comm,h (normalized priority from engagement)
};

double compute_P_equity(const HexTreeData& h, double lambda) {
    double gap = std::max(0.0, h.V_max - h.V_current);
    double base = h.UHI * std::fabs(h.alpha) * gap * h.feasible;
    double equity_term = std::exp(-lambda * h.SVI);
    return base * equity_term;
}

struct EquityBenefitResult {
    std::string hex_id;
    double P_equity;
    double benefit_per_dollar;
};

std::vector<EquityBenefitResult> compute_equity_benefits(
        const std::vector<HexTreeData>& hexes,
        double lambda) {
    std::vector<EquityBenefitResult> out;
    out.reserve(hexes.size());
    for (const auto& h : hexes) {
        double P_eq = compute_P_equity(h, lambda);
        double B = (h.cost_eff > 0.0) ? P_eq / h.cost_eff : 0.0;
        out.push_back({h.hex_id, P_eq, B});
    }
    return out;
}

double loss_for_lambda(const std::vector<HexTreeData>& hexes, double lambda) {
    // Compute normalized P_equity distribution and compare to community weights.
    std::vector<double> P;
    P.reserve(hexes.size());
    for (const auto& h : hexes) {
        P.push_back(compute_P_equity(h, lambda));
    }
    double Z = 0.0;
    for (double v : P) Z += v;
    if (Z <= 0.0) {
        return 1e9;
    }
    double loss = 0.0;
    for (std::size_t i = 0; i < hexes.size(); ++i) {
        double p_norm = P[i] / Z;
        double diff = p_norm - hexes[i].community_weight;
        loss += diff * diff;
    }
    return loss;
}

// Simple 1D search over λ to fit community priorities.
double calibrate_lambda(const std::vector<HexTreeData>& hexes,
                        double lambda_min,
                        double lambda_max,
                        int steps) {
    double best_lambda = lambda_min;
    double best_loss = std::numeric_limits<double>::infinity();

    for (int i = 0; i <= steps; ++i) {
        double lambda = lambda_min + (lambda_max - lambda_min) * static_cast<double>(i) / static_cast<double>(steps);
        double L = loss_for_lambda(hexes, lambda);
        if (L < best_loss) {
            best_loss = L;
            best_lambda = lambda;
        }
    }
    return best_lambda;
}

int main() {
    // Synthetic hex data with community priority weights.
    std::vector<HexTreeData> hexes = {
        {"hex_10_20", 7.0, -8.0, 0.6, 0.2, 1.0, 0.8, 100000.0, 0.4},
        {"hex_11_20", 5.5, -7.5, 0.5, 0.3, 1.0, 0.5, 90000.0, 0.3},
        {"hex_12_20", 6.0, -8.2, 0.7, 0.4, 0.8, 0.2, 110000.0, 0.3}
    };

    double lambda = calibrate_lambda(hexes, /*lambda_min=*/0.0, /*lambda_max=*/3.0, /*steps=*/60);
    std::cout << "Calibrated equity parameter λ = " << lambda << "\n";

    auto results = compute_equity_benefits(hexes, lambda);
    std::cout << "Equity-weighted benefits per dollar:\n";
    for (const auto& r : results) {
        std::cout << "Hex " << r.hex_id
                  << " | P_equity=" << r.P_equity
                  << " | benefit_per_dollar=" << r.benefit_per_dollar << "\n";
    }

    return 0;
}
