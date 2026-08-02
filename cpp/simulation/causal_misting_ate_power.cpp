// File: cpp/simulation/causal_misting_ate_power.cpp

#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <stdexcept>
#include <string>

// Simple data structure for MRT observations under MARL interventions.
struct MistingObservation {
    bool mist_on;    // treatment indicator: 1 = misting, 0 = no misting
    double mrt;      // measured mean radiant temperature [°C]
};

// Estimate ATE = E[MRT | mist=1] - E[MRT | mist=0] and its 95% CI.
struct ATEResult {
    double ate;
    double ci_lower;
    double ci_upper;
};

class ATEEstimator {
public:
    ATEResult estimate(const std::vector<MistingObservation>& obs) const {
        double sum_on = 0.0, sum_off = 0.0;
        double sumsq_on = 0.0, sumsq_off = 0.0;
        int n_on = 0, n_off = 0;

        for (const auto& o : obs) {
            if (o.mist_on) {
                sum_on += o.mrt;
                sumsq_on += o.mrt * o.mrt;
                n_on++;
            } else {
                sum_off += o.mrt;
                sumsq_off += o.mrt * o.mrt;
                n_off++;
            }
        }

        if (n_on < 2 || n_off < 2) {
            throw std::runtime_error("Insufficient samples for ATE estimation.");
        }

        double mean_on = sum_on / n_on;
        double mean_off = sum_off / n_off;
        double var_on = (sumsq_on / n_on) - mean_on * mean_on;
        double var_off = (sumsq_off / n_off) - mean_off * mean_off;

        double ate = mean_on - mean_off;

        // Standard error of difference in means (assuming independence).
        double se = std::sqrt(var_on / n_on + var_off / n_off);

        // 95% CI using normal approximation: ate ± 1.96 * se
        double z = 1.96;
        double lower = ate - z * se;
        double upper = ate + z * se;

        return {ate, lower, upper};
    }

    // Check gate condition: 95% CI strictly negative (misting reduces MRT).
    bool gate_domain_performance_ok(const ATEResult& res, std::string& reason) const {
        if (res.ci_upper < 0.0) {
            reason.clear();
            return true;
        }
        reason = "ATE 95% CI is not strictly negative; misting effect on MRT is uncertain or non-beneficial.";
        return false;
    }

    // Compute required sample size per group for power 0.8 at alpha=0.05, given expected effect and variance.
    // n_per_group ≈ 2 * (z_(1-α/2) + z_(power))^2 * σ^2 / Δ^2
    int required_sample_size(double expected_effect_degC,
                             double std_dev_degC,
                             double alpha,
                             double power) const
    {
        double z_alpha = z_from_prob(1.0 - alpha / 2.0);
        double z_power = z_from_prob(power);
        double delta = std::abs(expected_effect_degC);
        double sigma = std_dev_degC;

        if (delta <= 0.0 || sigma <= 0.0) {
            throw std::runtime_error("Invalid expected effect or std dev for sample size calculation.");
        }

        double n = 2.0 * std::pow(z_alpha + z_power, 2.0) * sigma * sigma / (delta * delta);
        return static_cast<int>(std::ceil(n));
    }

private:
    // Approximate inverse CDF for standard normal via simple binary search on a table
    // (in practice, use a robust implementation; here we hardcode key values).
    static double z_from_prob(double p) {
        // For commonly used values we can return known z-scores.
        if (std::abs(p - 0.95) < 1e-3) return 1.645;
        if (std::abs(p - 0.975) < 1e-3) return 1.96;
        if (std::abs(p - 0.8) < 1e-3) return 0.84;
        if (std::abs(p - 0.9) < 1e-3) return 1.28;
        // Fallback: near 0.5 -> ~0
        if (std::abs(p - 0.5) < 1e-3) return 0.0;
        // Simple linear approximation for demonstration
        double diff = p - 0.5;
        return diff * 3.0; // coarse slope
    }
};

int main() {
    // Synthetic intervention data: MARL controller toggles misting across corridors.
    // Suppose misting reduces MRT by ~2°C on average with std dev ~1.5°C.
    std::vector<MistingObservation> obs;
    std::mt19937 rng(42);
    std::normal_distribution<double> base_mrt(40.0, 1.5);    // no-mist baseline
    std::normal_distribution<double> effect(-2.0, 0.5);      // misting effect

    int n_per_group = 50; // example sample size
    for (int i = 0; i < n_per_group; ++i) {
        double mrt_off = base_mrt(rng);
        obs.push_back({false, mrt_off});
    }
    for (int i = 0; i < n_per_group; ++i) {
        double mrt_on = base_mrt(rng) + effect(rng);
        obs.push_back({true, mrt_on});
    }

    ATEEstimator estimator;
    ATEResult res = estimator.estimate(obs);

    std::cout << "Estimated ATE (MRT mist_on - mist_off) = " << res.ate << " °C\n";
    std::cout << "95% CI = [" << res.ci_lower << ", " << res.ci_upper << "] °C\n";

    std::string reason;
    bool domain_ok = estimator.gate_domain_performance_ok(res, reason);
    std::cout << "domain_performance_ok: " << (domain_ok ? "true" : "false") << "\n";
    if (!domain_ok) {
        std::cout << "Reason: " << reason << "\n";
    }

    // Sample size calculation for power 0.8, alpha 0.05, expected effect -2°C, std dev 1.5°C.
    double expected_effect_degC = -2.0;
    double std_dev_degC = 1.5;
    double alpha = 0.05;
    double power = 0.8;

    int required_n = estimator.required_sample_size(expected_effect_degC,
                                                    std_dev_degC, alpha, power);
    std::cout << "Required sample size per group for power 0.8 at alpha=0.05: n ≈ "
              << required_n << "\n";

    return 0;
}
