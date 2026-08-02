// File: cpp/simulation/hex_monte_carlo_priority_sensitivity.cpp

#include <random>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <cmath>
#include "../eco_restoration/hex_priority.hpp"

using namespace hex_analytics;

/**
 * 48. Hex-based Monte Carlo sensitivity analysis on α, β, γ and NDVI/NDBI/NDWI.
 *
 * Concept:
 *  - If α, β, γ are estimated via Bayesian regression, we have posterior
 *    distributions p(α), p(β), p(γ). Hex-level NDVI/NDBI/NDWI (V_h, B_h, W_h)
 *    also carry uncertainty (e.g., sensor error, temporal variability).
 *  - We define tree/roof/water priority functions:
 *
 *      tree_priority_h  = f_tree(α, V_h, equity_h, cost_h)
 *      roof_priority_h  = f_roof(β, B_h, equity_h, cost_h)
 *      water_priority_h = f_water(γ, W_h, equity_h, cost_h)
 *
 *  - A Monte Carlo framework samples from these distributions and propagates
 *    uncertainties to priority scores, generating distributions over
 *    ranking order across hexes.[189][196][199]
 *  - Hexes whose rank is stable (e.g., hex A ranked in top 5 in >90% of
 *    simulations) are robust intervention targets; those with highly
 *    variable rank are uncertain and may require more data or flexible plans.
 *
 * This file encodes a C++ Monte Carlo engine analogous to the requested Rust
 * implementation, suitable for inclusion in Prometheus-Praxis.
 */

struct HexState {
    std::string hex_id;
    double V_mean;
    double B_mean;
    double W_mean;
    double V_std;
    double B_std;
    double W_std;
    double equity_weight;
    double cost;
};

struct PosteriorParams {
    double alpha_mean;
    double beta_mean;
    double gamma_mean;
    double alpha_std;
    double beta_std;
    double gamma_std;
};

struct PriorityScores {
    std::string hex_id;
    double tree_priority;
    double roof_priority;
    double water_priority;
    double combined_priority;
};

double sample_normal(double mean, double stddev, std::mt19937& gen) {
    if (stddev <= 0.0) return mean;
    std::normal_distribution<double> dist(mean, stddev);
    return dist(gen);
}

PriorityScores compute_priorities_sample(const HexState& h,
                                         double alpha_s,
                                         double beta_s,
                                         double gamma_s,
                                         std::mt19937& gen) {
    double V_s = sample_normal(h.V_mean, h.V_std, gen);
    double B_s = sample_normal(h.B_mean, h.B_std, gen);
    double W_s = sample_normal(h.W_mean, h.W_std, gen);

    double tree_priority  = h.equity_weight * std::fabs(alpha_s) * V_s / std::max(h.cost, 1e-3);
    double roof_priority  = h.equity_weight * std::fabs(beta_s)  * B_s / std::max(h.cost, 1e-3);
    double water_priority = h.equity_weight * std::fabs(gamma_s) * W_s / std::max(h.cost, 1e-3);

    double combined = tree_priority + roof_priority + water_priority;

    return {h.hex_id, tree_priority, roof_priority, water_priority, combined};
}

struct RankStats {
    std::string hex_id;
    int top_k_count;
    int simulations;
};

void run_monte_carlo(const std::vector<HexState>& hexes,
                     const PosteriorParams& post,
                     int simulations,
                     int k_top,
                     std::vector<RankStats>& out_stats) {
    std::mt19937 gen(42);
    out_stats.clear();
    out_stats.reserve(hexes.size());
    for (const auto& h : hexes) {
        out_stats.push_back({h.hex_id, 0, simulations});
    }

    std::normal_distribution<double> dist_alpha(post.alpha_mean, post.alpha_std);
    std::normal_distribution<double> dist_beta(post.beta_mean, post.beta_std);
    std::normal_distribution<double> dist_gamma(post.gamma_mean, post.gamma_std);

    for (int s = 0; s < simulations; ++s) {
        double alpha_s = dist_alpha(gen);
        double beta_s  = dist_beta(gen);
        double gamma_s = dist_gamma(gen);

        std::vector<PriorityScores> scores;
        scores.reserve(hexes.size());
        for (const auto& h : hexes) {
            scores.push_back(compute_priorities_sample(h, alpha_s, beta_s, gamma_s, gen));
        }

        std::sort(scores.begin(), scores.end(),
                  [](const PriorityScores& a, const PriorityScores& b) {
                      return a.combined_priority > b.combined_priority;
                  });

        for (int i = 0; i < std::min(k_top, static_cast<int>(scores.size())); ++i) {
            const std::string& id = scores[i].hex_id;
            for (auto& rs : out_stats) {
                if (rs.hex_id == id) {
                    rs.top_k_count += 1;
                    break;
                }
            }
        }
    }
}

int main_monte_carlo_priority() {
    std::vector<HexState> hexes = {
        {"hex_10_20", 0.35, 0.50, 0.05, 0.02, 0.03, 0.01, 0.9, 1.0},
        {"hex_11_20", 0.40, 0.45, 0.08, 0.02, 0.02, 0.01, 0.8, 1.1},
        {"hex_12_20", 0.25, 0.60, 0.02, 0.03, 0.03, 0.01, 1.0, 0.9}
    };

    PosteriorParams post;
    post.alpha_mean = -8.0;
    post.beta_mean  = 3.0;
    post.gamma_mean = -5.0;
    post.alpha_std  = 0.5;
    post.beta_std   = 0.3;
    post.gamma_std  = 0.4;

    int simulations = 2000;
    int k_top = 2;

    std::vector<RankStats> stats;
    run_monte_carlo(hexes, post, simulations, k_top, stats);

    std::cout << "Monte Carlo robustness of top-" << k_top << " rankings:\n";
    for (const auto& rs : stats) {
        double freq = static_cast<double>(rs.top_k_count) / rs.simulations;
        std::cout << "  " << rs.hex_id << " in top-" << k_top
                  << " in " << freq * 100.0 << "% of simulations\n";
    }

    return 0;
}
