// File: cpp/eco_restoration/delta_vt_gp_threshold_tuner.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>

// Note: This implementation avoids external libraries and uses hand-rolled
// linear algebra instead of Eigen; parameters α, β, γ are tuned via a basic
// Gaussian-process-inspired Bayesian optimisation loop.

namespace eco {

struct HexDriftSample {
    std::string hex_id;
    double alpha;
    double beta;
    double gamma;
    double observed_breach_rate; // empirical breach probability under these params
};

struct CorridorTuningRecommendation {
    std::string hex_id;
    double alpha_opt;
    double beta_opt;
    double gamma_opt;
    double target_breach_prob;
};

double squared_distance(const HexDriftSample& a, const HexDriftSample& b) {
    double da = a.alpha - b.alpha;
    double db = a.beta - b.beta;
    double dg = a.gamma - b.gamma;
    return da*da + db*db + dg*dg;
}

// Simple squared exponential kernel.
double kernel(const HexDriftSample& a, const HexDriftSample& b, double length_scale, double sigma_f) {
    double r2 = squared_distance(a, b);
    return sigma_f * sigma_f * std::exp(-0.5 * r2 / (length_scale * length_scale));
}

// Basic GP posterior mean at candidate x using samples (no matrix library).
double gp_posterior_mean(const HexDriftSample& x,
                         const std::vector<HexDriftSample>& samples,
                         double length_scale,
                         double sigma_f,
                         double sigma_n) {
    std::size_t n = samples.size();
    if (n == 0) return 0.0;

    // Build K matrix and k vector.
    std::vector<std::vector<double>> K(n, std::vector<double>(n, 0.0));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            K[i][j] = kernel(samples[i], samples[j], length_scale, sigma_f);
            if (i == j) {
                K[i][j] += sigma_n * sigma_n;
            }
        }
    }
    std::vector<double> k(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        k[i] = kernel(samples[i], x, length_scale, sigma_f);
    }

    // Solve K * alpha_vec = y via naive Gaussian elimination.
    std::vector<double> y(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        y[i] = samples[i].observed_breach_rate;
    }

    // Augmented matrix [K | y]
    for (std::size_t i = 0; i < n; ++i) {
        K[i].push_back(y[i]);
    }

    // Gaussian elimination
    for (std::size_t i = 0; i < n; ++i) {
        // Pivot
        double pivot = K[i][i];
        if (std::fabs(pivot) < 1e-12) continue;
        for (std::size_t j = i; j < n + 1; ++j) {
            K[i][j] /= pivot;
        }
        // Eliminate
        for (std::size_t r = 0; r < n; ++r) {
            if (r == i) continue;
            double factor = K[r][i];
            for (std::size_t c = i; c < n + 1; ++c) {
                K[r][c] -= factor * K[i][c];
            }
        }
    }

    std::vector<double> alpha_vec(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        alpha_vec[i] = K[i][n];
    }

    // Posterior mean: k^T alpha_vec
    double mean = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        mean += k[i] * alpha_vec[i];
    }
    return mean;
}

// Simple Bayesian optimisation loop: sample around current parameters,
// evaluate empirical breach rate (simulated), and move toward target.
CorridorTuningRecommendation tune_hex_corridor(const std::string& hex_id,
                                               double alpha_init,
                                               double beta_init,
                                               double gamma_init,
                                               double target_breach_prob) {
    std::vector<HexDriftSample> samples;

    // Initial sample
    HexDriftSample s0{hex_id, alpha_init, beta_init, gamma_init,
                      /*observed_breach_rate=*/0.06}; // example empirical rate
    samples.push_back(s0);

    double length_scale = 0.1;
    double sigma_f = 0.05;
    double sigma_n = 0.01;

    double best_alpha = alpha_init;
    double best_beta = beta_init;
    double best_gamma = gamma_init;
    double best_diff = std::fabs(s0.observed_breach_rate - target_breach_prob);

    // Iterate a small number of steps
    for (int iter = 0; iter < 10; ++iter) {
        // Propose a new point via random perturbation around current best
        HexDriftSample cand{hex_id,
                            best_alpha + 0.02 * (iter % 3 - 1),
                            best_beta + 0.02 * ((iter+1) % 3 - 1),
                            best_gamma + 0.02 * ((iter+2) % 3 - 1),
                            0.0};

        // Predict breach rate using GP posterior mean
        double pred = gp_posterior_mean(cand, samples, length_scale, sigma_f, sigma_n);

        // Simulate observation by nudging toward prediction
        cand.observed_breach_rate = pred;

        samples.push_back(cand);

        double diff = std::fabs(cand.observed_breach_rate - target_breach_prob);
        if (diff < best_diff) {
            best_diff = diff;
            best_alpha = cand.alpha;
            best_beta = cand.beta;
            best_gamma = cand.gamma;
        }
    }

    CorridorTuningRecommendation rec{};
    rec.hex_id = hex_id;
    rec.alpha_opt = best_alpha;
    rec.beta_opt = best_beta;
    rec.gamma_opt = best_gamma;
    rec.target_breach_prob = target_breach_prob;
    return rec;
}

void print_corridor_tuning_sql(const CorridorTuningRecommendation& rec) {
    std::cout << "INSERT INTO corridor_tuning_recommendation "
              << "(hex_id, alpha_opt, beta_opt, gamma_opt, target_breach_prob) VALUES ('"
              << rec.hex_id << "', "
              << rec.alpha_opt << ", "
              << rec.beta_opt << ", "
              << rec.gamma_opt << ", "
              << rec.target_breach_prob << ");\n";
}

} // namespace eco

int main() {
    using namespace eco;

    // Example tuning for one Phoenix hex.
    std::string hex_id = "hex_PHX_001";
    double alpha_init = 0.5;
    double beta_init = 0.01;
    double gamma_init = 0.1;
    double target_breach_prob = 0.05;

    CorridorTuningRecommendation rec =
        tune_hex_corridor(hex_id, alpha_init, beta_init, gamma_init, target_breach_prob);
    print_corridor_tuning_sql(rec);

    return 0;
}
