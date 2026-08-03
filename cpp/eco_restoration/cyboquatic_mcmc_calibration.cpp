// File: cpp/eco_restoration/cyboquatic_mcmc_calibration.cpp
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <limits>

namespace eco {

// Historical stormwater observation at a time point.
struct StormwaterObservation {
    double Q;       // discharge
    double C;       // pollutant concentration
    double T;       // temperature
    double C_obs;   // observed pollutant concentration
};

// Simulator parameters to calibrate (simplified subset).
struct SimulatorParams {
    double alpha_Q;
    double k_decay;
    double k_flush_base;
    double k_T;

    double log_likelihood; // current log-likelihood
};

// Simple canal simulator: one-step update of C given Q and parameters.
double simulate_C_next(double C, double Q, const SimulatorParams& p) {
    double k_flush_Q = p.k_flush_base + 0.001 * Q;
    double C_next = C * std::exp(-(p.k_decay + k_flush_Q)) + 0.0; // ignore inflow for calibration
    return C_next;
}

// Compute log-likelihood of parameters given historical data.
double log_likelihood(const std::vector<StormwaterObservation>& data,
                      const SimulatorParams& p) {
    double ll = 0.0;
    double C_model = data.front().C;
    double sigma = 0.05; // assumed observation noise

    for (std::size_t t = 0; t < data.size(); ++t) {
        double Q = data[t].Q;
        double C_obs = data[t].C_obs;
        C_model = simulate_C_next(C_model, Q, p);
        double diff = C_obs - C_model;
        ll += -0.5 * (diff * diff) / (sigma * sigma);
    }
    return ll;
}

// Random walk proposal for MCMC.
SimulatorParams propose(const SimulatorParams& current,
                        std::mt19937& rng,
                        double step_scale) {
    std::normal_distribution<double> norm(0.0, step_scale);
    SimulatorParams prop = current;
    prop.alpha_Q      = current.alpha_Q      + norm(rng);
    prop.k_decay      = current.k_decay      + norm(rng);
    prop.k_flush_base = current.k_flush_base + norm(rng);
    prop.k_T          = current.k_T          + norm(rng);

    // Enforce simple physical bounds.
    if (prop.alpha_Q < 0.0) prop.alpha_Q = 0.0;
    if (prop.k_decay < 0.0) prop.k_decay = 0.0;
    if (prop.k_flush_base < 0.0) prop.k_flush_base = 0.0;
    if (prop.k_T < 0.0) prop.k_T = 0.0;

    return prop;
}

// Metropolis-Hastings MCMC to sample posterior over simulator parameters.
SimulatorParams run_mcmc(const std::vector<StormwaterObservation>& data,
                         int iterations,
                         double step_scale) {
    std::random_device rd;
    std::mt19937 rng(rd());

    SimulatorParams current{};
    current.alpha_Q = 0.3;
    current.k_decay = 0.05;
    current.k_flush_base = 0.03;
    current.k_T = 0.1;
    current.log_likelihood = log_likelihood(data, current);

    SimulatorParams best = current;

    std::uniform_real_distribution<double> uni(0.0, 1.0);

    for (int it = 0; it < iterations; ++it) {
        SimulatorParams prop = propose(current, rng, step_scale);
        double ll_prop = log_likelihood(data, prop);
        double accept_ratio = std::exp(ll_prop - current.log_likelihood);
        if (accept_ratio >= 1.0 || uni(rng) < accept_ratio) {
            prop.log_likelihood = ll_prop;
            current = prop;
            if (ll_prop > best.log_likelihood) {
                best = prop;
            }
        }
    }

    return best;
}

// Emit SQL update for cyboquatic_calibration_profile.
void print_calibration_sql(const SimulatorParams& p, const std::string& profile_id) {
    std::cout << "UPDATE cyboquatic_calibration_profile SET "
              << "alpha_Q = " << p.alpha_Q << ", "
              << "k_decay = " << p.k_decay << ", "
              << "k_flush_base = " << p.k_flush_base << ", "
              << "k_T = " << p.k_T << " "
              << "WHERE profile_id = '" << profile_id << "';\n";
}

} // namespace eco

int main() {
    using namespace eco;

    // Example synthetic historical stormwater data.
    std::vector<StormwaterObservation> data;
    double C0 = 1.2;
    double C_true = C0;
    SimulatorParams true_p{};
    true_p.alpha_Q = 0.3;
    true_p.k_decay = 0.06;
    true_p.k_flush_base = 0.035;
    true_p.k_T = 0.11;

    std::mt19937 rng(1234);
    std::normal_distribution<double> noise(0.0, 0.05);

    for (int t = 0; t < 50; ++t) {
        double Q = 80.0 + 10.0 * std::sin(t * 3.141592653589793 / 10.0);
        double T = 300.0; // unused in simplified example
        double C_obs = simulate_C_next(C_true, Q, true_p) + noise(rng);
        C_true = C_obs;
        data.push_back({Q, C0, T, C_obs});
    }

    SimulatorParams best = run_mcmc(data, /*iterations=*/1000, /*step_scale=*/0.01);

    std::cout << "Best posterior parameter estimate:\n";
    std::cout << "  alpha_Q = " << best.alpha_Q << "\n";
    std::cout << "  k_decay = " << best.k_decay << "\n";
    std::cout << "  k_flush_base = " << best.k_flush_base << "\n";
    std::cout << "  k_T = " << best.k_T << "\n";
    std::cout << "  log_likelihood = " << best.log_likelihood << "\n\n";

    print_calibration_sql(best, "phoenix_canal_profile_001");

    return 0;
}
