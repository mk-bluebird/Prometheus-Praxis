// File: cpp/eco_restoration/drift_prediction_and_roh_aggregation.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>

namespace praxis {
namespace eco {

// ----------------------------------------------------------
// 21. Data-driven calibration drift model for PFAS electrodes
// ----------------------------------------------------------
//
// We model calibration drift D(t) (in %/hr or % total) as a function of
// environmental covariates:
//   temperature (T), humidity (H), usage intensity (U).
//
// A simple linear-Gaussian model:
//   D = beta_0 + beta_T*T + beta_H*H + beta_U*U + epsilon,
//   epsilon ~ N(0, sigma_eps^2).
//
// The predictive distribution after 30 days is:
//   D_30 ~ N( mu_30, sigma_30^2 ), where
//   mu_30 = beta_0 + beta_T*T_bar_30 + beta_H*H_bar_30 + beta_U*U_bar_30
//   sigma_30^2 = sigma_eps^2 + model_uncertainty.
//
// Reliability-token expiry can be set so that the probability of drift exceeding
// the allowed limit is below a chosen threshold (e.g., 5%), using the predictive
// distribution.

struct DriftSample {
    double temp;     // temperature (°C)
    double humidity; // relative humidity [0,1]
    double usage;    // usage intensity [0,1]
    double drift;    // observed drift (%/hr or normalized)
};

struct DriftModelParams {
    double beta_0;
    double beta_T;
    double beta_H;
    double beta_U;
    double sigma_eps;
};

double predict_drift(const DriftModelParams& p,
                     double temp,
                     double humidity,
                     double usage) {
    return p.beta_0 + p.beta_T * temp + p.beta_H * humidity + p.beta_U * usage;
}

// Compute approximate predictive mean and variance after 30 days given
// average covariates over the period.
struct DriftPrediction {
    double mu_30;
    double sigma_30;
};

// For simplicity, we treat sigma_30 ≈ sigma_eps (ignoring model parameter uncertainty),
// but in practice it would include parameter posterior variance.
DriftPrediction predictive_drift_30_days(const DriftModelParams& p,
                                         double temp_avg_30,
                                         double humidity_avg_30,
                                         double usage_avg_30) {
    double mu_30 = predict_drift(p, temp_avg_30, humidity_avg_30, usage_avg_30);
    double sigma_30 = p.sigma_eps;
    return DriftPrediction{mu_30, sigma_30};
}

// Given a drift limit L (e.g., 2%/hr) and predictive N(mu_30, sigma_30^2),
// set reliability-token expiry horizon so that P(D > L) <= alpha (e.g., 0.05).
//
// For N(mu, sigma^2), P(D > L) = 1 - Phi((L - mu)/sigma).
// We can invert this to choose expiry earlier if mu_30 is close to L.
// Here we compute the exceedance probability for inspection.
double exceedance_probability(double mu, double sigma, double limit) {
    double z = (limit - mu) / sigma;
    // Approximate Phi(z) using erf.
    double phi = 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
    return 1.0 - phi;
}

// ----------------------------------------------------------
// 22. RoH aggregation across corridors
// ----------------------------------------------------------
//
// We define RoH values roh_i ∈ [0,1] for hex-cells.
// A spatial aggregation operator ⊕ combines them into corridor RoH:
//
//   RoH_corridor = ⊕_{i ∈ corridor} roh_i.
//
// To ensure that the ALN invariant "corridor RoH ≤ 0.30" implies each
// sub-component is safe under any partition, ⊕ must satisfy:
//
//   - Monotonicity: if roh_i <= roh'_i for all i, then ⊕ roh_i <= ⊕ roh'_i.
//   - Idempotence over safe bounds: if roh_i ≤ 0.30 for all i, then
//       ⊕ roh_i ≤ 0.30 and we prefer an operator that does not mask unsafe
//       cells via averaging.
//   - Partition safety: if ⊕ roh_i <= 0.30 and ⊕ is "max", then each roh_i
//       <= RoH_corridor <= 0.30, so any sub-corridor inherits safety.
//
// Choosing:
//   ⊕(roh_1, ..., roh_n) = max_i roh_i
//
// This operator satisfies monotonicity and ensures that corridor RoH is the
// worst-case sub-cell RoH; thus corridor RoH ≤ 0.30 implies roh_i ≤ 0.30
// for all i and any partition.

double roh_aggregate_max(const std::vector<double>& roh_cells) {
    double m = 0.0;
    for (double r : roh_cells) {
        if (r > m) m = r;
    }
    return m;
}

bool corridor_safe(const std::vector<double>& roh_cells, double threshold) {
    double roh_corridor = roh_aggregate_max(roh_cells);
    return roh_corridor <= threshold;
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 21. Drift prediction demo.
    DriftModelParams params{
        0.1,   // beta_0
        0.002, // beta_T (per °C)
        0.05,  // beta_H (per humidity fraction)
        0.10,  // beta_U (per usage intensity)
        0.02   // sigma_eps (std dev)
    };

    double temp_avg_30     = 42.0; // hot Phoenix average over 30 days
    double humidity_avg_30 = 0.25;
    double usage_avg_30    = 0.7;

    DriftPrediction pred = predictive_drift_30_days(params,
                                                    temp_avg_30,
                                                    humidity_avg_30,
                                                    usage_avg_30);

    double drift_limit = 0.02; // allowed drift (e.g., 2%/hr)
    double p_exceed = exceedance_probability(pred.mu_30, pred.sigma_30, drift_limit);

    std::cout << "Data-driven calibration drift prediction (30 days):\n";
    std::cout << "  mu_30=" << pred.mu_30 << ", sigma_30=" << pred.sigma_30 << "\n";
    std::cout << "  Drift limit L=" << drift_limit << "\n";
    std::cout << "  P(D_30 > L) ≈ " << p_exceed * 100.0 << "%\n";
    std::cout << "  Reliability-token expiry should be set so that this exceedance "
              << "probability remains below the policy threshold (e.g., <5%).\n\n";

    // 22. RoH aggregation demo.
    std::vector<double> roh_cells{0.22, 0.18, 0.27, 0.29};
    double roh_corridor = roh_aggregate_max(roh_cells);
    double roh_threshold = 0.30;

    std::cout << "RoH aggregation across corridor (max operator):\n";
    std::cout << "  Cell RoH values: ";
    for (double r : roh_cells) {
        std::cout << r << " ";
    }
    std::cout << "\n  Aggregated corridor RoH (⊕=max): " << roh_corridor << "\n";
    std::cout << "  Corridor safe (RoH_corridor <= 0.30)? "
              << (corridor_safe(roh_cells, roh_threshold) ? "YES" : "NO") << "\n";
    std::cout << "  With ⊕=max, corridor safety implies each sub-cell RoH <= 0.30 "
              << "under any partition, satisfying ALN invariants.\n";

    return 0;
}

} // namespace eco
} // namespace praxis
