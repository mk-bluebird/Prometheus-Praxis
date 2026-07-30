// filename: ecorestorationshard/ecosafety_core_v2/cpp/ker_residual_early_warning.hpp
// destination: ecorestorationshard/ecosafety_core_v2/cpp/ker_residual_early_warning.hpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
//
// Purpose:
//   Non-actuating C++ header to:
//     - Compute AR(1) coefficient and variance of V_t over 30-day windows.
//     - Record early-warning indicators before KER lane changes.
//   This skeleton uses standard formulas for AR(1) and variance.

#ifndef ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_KER_RESIDUAL_EARLY_WARNING_HPP
#define ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_KER_RESIDUAL_EARLY_WARNING_HPP

#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>

namespace ecosafety_core_v2 {

struct ResidualWindowSample {
    std::string segment_id;
    std::string yyyymmdd;
    double      vt_residual;
    std::string lane; // RESEARCH, PILOT, PROD, BLOCKED
};

struct EarlyWarningIndicators {
    std::string segment_id;
    double      ar1_coeff;
    double      variance;
    bool        rising_ar1;
    bool        rising_variance;
};

inline EarlyWarningIndicators
compute_ar1_and_variance(const std::vector<ResidualWindowSample>& samples_prev,
                         const std::vector<ResidualWindowSample>& samples_curr)
{
    if (samples_curr.size() < 2) {
        throw std::invalid_argument("Need at least 2 samples for AR(1)");
    }

    // Compute AR(1) for current window.
    double sum_x = 0.0, sum_xlag = 0.0, sum_xxlag = 0.0, sum_xlag2 = 0.0;
    const std::size_t N = samples_curr.size();
    for (std::size_t t = 1; t < N; ++t) {
        const double x    = samples_curr[t].vt_residual;
        const double xlag = samples_curr[t - 1].vt_residual;
        sum_x     += x;
        sum_xlag  += xlag;
        sum_xxlag += x * xlag;
        sum_xlag2 += xlag * xlag;
    }
    const double n = static_cast<double>(N - 1);
    const double num   = n * sum_xxlag - sum_x * sum_xlag;
    const double denom = n * sum_xlag2 - sum_xlag * sum_xlag;
    double ar1_curr = 0.0;
    if (std::fabs(denom) > 1e-9) {
        ar1_curr = num / denom;
    }

    // Variance for current window.
    double mean_curr = 0.0;
    for (const auto& s : samples_curr) {
        mean_curr += s.vt_residual;
    }
    mean_curr /= static_cast<double>(samples_curr.size());

    double var_curr = 0.0;
    for (const auto& s : samples_curr) {
        const double diff = s.vt_residual - mean_curr;
        var_curr += diff * diff;
    }
    var_curr /= static_cast<double>(samples_curr.size());

    // Previous window indicators (optional, for trend).
    double ar1_prev = 0.0;
    double var_prev = 0.0;
    if (!samples_prev.empty()) {
        // Compute AR(1) and variance for previous window similarly.
        const std::size_t Np = samples_prev.size();
        if (Np >= 2) {
            double sum_xp = 0.0, sum_xlagp = 0.0, sum_xxlagp = 0.0, sum_xlag2p = 0.0;
            for (std::size_t t = 1; t < Np; ++t) {
                const double x    = samples_prev[t].vt_residual;
                const double xlag = samples_prev[t - 1].vt_residual;
                sum_xp     += x;
                sum_xlagp  += xlag;
                sum_xxlagp += x * xlag;
                sum_xlag2p += xlag * xlag;
            }
            const double np   = static_cast<double>(Np - 1);
            const double nump = np * sum_xxlagp - sum_xp * sum_xlagp;
            const double denp = np * sum_xlag2p - sum_xlagp * sum_xlagp;
            if (std::fabs(denp) > 1e-9) {
                ar1_prev = nump / denp;
            }

            double mean_prev = 0.0;
            for (const auto& s : samples_prev) {
                mean_prev += s.vt_residual;
            }
            mean_prev /= static_cast<double>(Np);

            for (const auto& s : samples_prev) {
                const double diff = s.vt_residual - mean_prev;
                var_prev += diff * diff;
            }
            var_prev /= static_cast<double>(Np);
        }
    }

    EarlyWarningIndicators out;
    out.segment_id     = samples_curr.empty() ? "" : samples_curr.front().segment_id;
    out.ar1_coeff      = ar1_curr;
    out.variance       = var_curr;
    out.rising_ar1     = (ar1_curr > ar1_prev);
    out.rising_variance= (var_curr > var_prev);
    return out;
}

} // namespace ecosafety_core_v2

#endif // ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_KER_RESIDUAL_EARLY_WARNING_HPP
