// File: cpp/eco_restoration/hex_anchor_invariance_metric.cpp

#include <vector>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <iostream>

/**
 * Hex-anchor NDVI–LST invariance analysis.
 *
 * We treat each hex anchor h as having a historical set of observations:
 *   NDVI_t, LST_t, time_t, season_t, year_t
 *
 * The question is: under what conditions is the NDVI–LST relationship at h
 * stable across seasons and years despite urban development?
 *
 * We operationalize this by:
 * 1. Fitting linear NDVI–LST regressions per epoch (e.g., summer 2020, winter 2020, summer 2030).
 * 2. Computing an invariance metric that penalizes drift in slope and intercept
 *    relative to a baseline while accounting for residual noise.
 *
 * The core invariance metric for a hex anchor h is:
 *
 *   I_h = exp( - ( |Δm| / σ_m_ref )^2 ) * exp( - ( |Δb| / σ_b_ref )^2 ) * (1 - R_drift)
 *
 * where:
 *   m_ref, b_ref   : baseline slope and intercept (e.g., first epoch).
 *   m_curr, b_curr : current epoch slope and intercept.
 *   Δm = m_curr - m_ref, Δb = b_curr - b_ref.
 *   σ_m_ref, σ_b_ref : reference uncertainties from baseline regression.
 *   R_drift : normalized change in residual RMSE between epochs.
 *
 * Values close to 1 indicate strong invariance; values near 0 indicate
 * strong structural change (e.g., due to substantial urbanization).
 */

struct NDVILSTSample {
    double ndvi;     // spectral index (e.g., NDVI)
    double lst;      // land surface temperature (e.g., Kelvin or Celsius)
    int year;        // e.g., 2020, 2030
    int season;      // 0 = winter, 1 = summer (simplified)
};

struct RegressionResult {
    double slope;
    double intercept;
    double rmse;
    double r2;
    std::size_t n;
};

class HexAnchorInvariance {
public:
    // Fit regression for a given set of samples.
    static RegressionResult fit_regression(const std::vector<NDVILSTSample>& samples) {
        if (samples.size() < 2) {
            throw std::invalid_argument("Need at least two samples for regression.");
        }

        double sum_x = 0.0;
        double sum_y = 0.0;
        double sum_xx = 0.0;
        double sum_xy = 0.0;
        std::size_t n = samples.size();

        for (const auto& s : samples) {
            double x = s.ndvi;
            double y = s.lst;
            sum_x += x;
            sum_y += y;
            sum_xx += x * x;
            sum_xy += x * y;
        }

        double mean_x = sum_x / static_cast<double>(n);
        double mean_y = sum_y / static_cast<double>(n);
        double denom = sum_xx - static_cast<double>(n) * mean_x * mean_x;
        if (std::fabs(denom) < 1e-12) {
            throw std::runtime_error("Degenerate NDVI variance for regression.");
        }

        double slope = (sum_xy - static_cast<double>(n) * mean_x * mean_y) / denom;
        double intercept = mean_y - slope * mean_x;

        double ss_tot = 0.0;
        double ss_res = 0.0;
        for (const auto& s : samples) {
            double y_hat = slope * s.ndvi + intercept;
            double diff = s.lst - y_hat;
            ss_res += diff * diff;
            double y_centered = s.lst - mean_y;
            ss_tot += y_centered * y_centered;
        }

        double rmse = std::sqrt(ss_res / static_cast<double>(n));
        double r2 = (ss_tot > 0.0) ? 1.0 - (ss_res / ss_tot) : 0.0;

        RegressionResult res;
        res.slope = slope;
        res.intercept = intercept;
        res.rmse = rmse;
        res.r2 = r2;
        res.n = n;
        return res;
    }

    /**
     * Compute invariance metric between a baseline epoch and a current epoch.
     *
     * baseline_samples: e.g., all summer+winter samples from 2020.
     * current_samples : e.g., all summer+winter samples from 2030.
     *
     * Optionally, users can filter by season and compare summer-to-summer
     * and winter-to-winter invariance separately to enforce temporal coherence.
     */
    static double compute_invariance(const std::vector<NDVILSTSample>& baseline_samples,
                                     const std::vector<NDVILSTSample>& current_samples) {
        RegressionResult baseline = fit_regression(baseline_samples);
        RegressionResult current  = fit_regression(current_samples);

        double delta_m = current.slope - baseline.slope;
        double delta_b = current.intercept - baseline.intercept;

        // Reference uncertainties: approximate via RMSE / NDVI range.
        double ndvi_min_ref = std::numeric_limits<double>::infinity();
        double ndvi_max_ref = -std::numeric_limits<double>::infinity();
        for (const auto& s : baseline_samples) {
            ndvi_min_ref = std::min(ndvi_min_ref, s.ndvi);
            ndvi_max_ref = std::max(ndvi_max_ref, s.ndvi);
        }
        double ndvi_range_ref = ndvi_max_ref - ndvi_min_ref;
        if (ndvi_range_ref <= 0.0) {
            ndvi_range_ref = 1.0; // avoid division by zero
        }

        double sigma_m_ref = baseline.rmse / ndvi_range_ref;
        double sigma_b_ref = baseline.rmse;

        double norm_delta_m = delta_m / sigma_m_ref;
        double norm_delta_b = delta_b / sigma_b_ref;

        // Drift in residual structure: normalized RMSE change.
        double rmse_ratio = (baseline.rmse > 0.0) ? current.rmse / baseline.rmse : 1.0;
        double R_drift = std::max(0.0, rmse_ratio - 1.0); // excess RMSE beyond baseline

        // Composite invariance score.
        double I_m = std::exp(-norm_delta_m * norm_delta_m);
        double I_b = std::exp(-norm_delta_b * norm_delta_b);
        double I_r = 1.0 / (1.0 + R_drift); // penalize increased RMSE

        double I = I_m * I_b * I_r;
        return I;
    }

    /**
     * Season-resolved invariance: compare summer-to-summer and winter-to-winter.
     *
     * This helps check whether the NDVI–LST slope/intercept remain stable
     * within the same seasonal regime, which is a stronger form of invariance
     * in climates like Phoenix that have strong seasonal contrasts.
     */
    static double compute_seasonal_invariance(const std::vector<NDVILSTSample>& baseline_samples,
                                              const std::vector<NDVILSTSample>& current_samples) {
        std::vector<NDVILSTSample> base_summer, base_winter;
        std::vector<NDVILSTSample> curr_summer, curr_winter;

        for (const auto& s : baseline_samples) {
            if (s.season == 1) base_summer.push_back(s);
            else base_winter.push_back(s);
        }
        for (const auto& s : current_samples) {
            if (s.season == 1) curr_summer.push_back(s);
            else curr_winter.push_back(s);
        }

        double I_summer = (base_summer.size() >= 2 && curr_summer.size() >= 2)
                          ? compute_invariance(base_summer, curr_summer)
                          : 0.0;
        double I_winter = (base_winter.size() >= 2 && curr_winter.size() >= 2)
                          ? compute_invariance(base_winter, curr_winter)
                          : 0.0;

        // Simple average; users can weight by sample count or by policy.
        return 0.5 * (I_summer + I_winter);
    }
};

// Example usage: compute invariance for a hex anchor using synthetic history.
int main() {
    std::vector<NDVILSTSample> baseline_2020;
    std::vector<NDVILSTSample> current_2030;

    // Synthetic baseline: 2020 samples.
    for (int i = 0; i < 50; ++i) {
        NDVILSTSample s;
        s.ndvi = 0.2 + 0.01 * i;
        s.lst  = 40.0 - 10.0 * s.ndvi + 0.5 * std::sin(0.1 * i); // inverse NDVI–LST relation
        s.year = 2020;
        s.season = (i % 2 == 0) ? 1 : 0; // alternating summer/winter for demo
        baseline_2020.push_back(s);
    }

    // Synthetic current: 2030 samples with slight urbanization-induced shift.
    for (int i = 0; i < 50; ++i) {
        NDVILSTSample s;
        s.ndvi = 0.18 + 0.01 * i; // slightly reduced NDVI due to infill development
        s.lst  = 41.0 - 9.5 * s.ndvi + 0.8 * std::sin(0.1 * i); // modest slope/intercept change
        s.year = 2030;
        s.season = (i % 2 == 0) ? 1 : 0;
        current_2030.push_back(s);
    }

    double I_all = HexAnchorInvariance::compute_invariance(baseline_2020, current_2030);
    double I_seasonal = HexAnchorInvariance::compute_seasonal_invariance(baseline_2020, current_2030);

    std::cout << "Hex-anchor NDVI–LST invariance (all seasons): " << I_all << "\n";
    std::cout << "Hex-anchor seasonal NDVI–LST invariance: " << I_seasonal << "\n";

    return 0;
}
