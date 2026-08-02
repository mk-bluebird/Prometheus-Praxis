// File: cpp/eco_restoration/hex_priority.hpp
// Data structures and function signatures for Monte Carlo priority analysis

#ifndef HEX_PRIORITY_HPP
#define HEX_PRIORITY_HPP

#include <string>
#include <vector>
#include <random>

namespace hex_analytics {

/**
 * State of a hex for priority scoring, including uncertainty.
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

/**
 * Posterior distribution parameters for α, β, γ coefficients.
 */
struct PosteriorParams {
    double alpha_mean;
    double beta_mean;
    double gamma_mean;
    double alpha_std;
    double beta_std;
    double gamma_std;
};

/**
 * Priority scores computed for a single sample.
 */
struct PriorityScores {
    std::string hex_id;
    double tree_priority;
    double roof_priority;
    double water_priority;
    double combined_priority;
};

/**
 * Statistics on ranking robustness from Monte Carlo simulations.
 */
struct RankStats {
    std::string hex_id;
    int top_k_count;
    int simulations;
};

/**
 * Sample from normal distribution with given mean and stddev.
 */
double sample_normal(double mean, double stddev, std::mt19937& gen);

/**
 * Compute priority scores for a single sample of parameters.
 */
PriorityScores compute_priorities_sample(const HexState& h,
                                         double alpha_s,
                                         double beta_s,
                                         double gamma_s,
                                         std::mt19937& gen);

/**
 * Run Monte Carlo sensitivity analysis on priority rankings.
 * 
 * @param hexes Input hex states with uncertainties
 * @param post Posterior parameters for α, β, γ
 * @param simulations Number of Monte Carlo iterations
 * @param k_top Size of "top-k" ranking to track
 * @param out_stats Output statistics for each hex
 */
void run_monte_carlo(const std::vector<HexState>& hexes,
                     const PosteriorParams& post,
                     int simulations,
                     int k_top,
                     std::vector<RankStats>& out_stats);

} // namespace hex_analytics

#endif // HEX_PRIORITY_HPP
