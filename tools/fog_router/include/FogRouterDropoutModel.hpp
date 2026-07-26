// File: ecorestorationshard/tools/fog_router/include/FogRouterDropoutModel.hpp

#pragma once

#include <vector>
#include <cmath>
#include <cstddef>
#include <stdexcept>

/**
 * Formal specification for FOG‑router predicate stability under sensor dropout.
 *
 * This header models:
 *  - A missing‑at‑random dropout process with probability p_d.
 *  - A Lyapunov residual V_t for the CEC plane.
 *  - A predicate `unmodeled_media_detected` that checks:
 *      * dropout run length >= L, and
 *      * V_t > V_FOG.
 *  - An approximate false‑positive probability under MAR.
 *
 * This is purely analytic and diagnostic: it does not actuate
 * hardware and relies only on standard C++.
 */

/**
 * Compute the probability of at least one dropout run
 * of length >= L in a window of T timesteps under a
 * simple MAR model with per‑step dropout probability p_d.
 *
 * Approximation:
 *   P(A_T) ≈ 1 - (1 - p_d^L)^(T - L + 1)
 *
 * Valid when p_d^L is small (rare long runs).
 */
inline double probability_dropout_run(std::size_t T, std::size_t L, double p_d)
{
    if (L == 0) {
        throw std::invalid_argument("Dropout length L must be > 0.");
    }
    if (T < L) {
        return 0.0;
    }
    if (p_d < 0.0 || p_d > 1.0 || !std::isfinite(p_d)) {
        throw std::invalid_argument("p_d must be in [0,1].");
    }

    const std::size_t n_positions = T - L + 1;
    const double base = std::pow(p_d, static_cast<double>(L));

    // If base is zero (p_d == 0), then probability is zero.
    if (base <= 0.0) {
        return 0.0;
    }

    const double one_minus_base = 1.0 - base;
    const double exponent = static_cast<double>(n_positions);

    // Guard against numerical issues.
    if (one_minus_base <= 0.0) {
        return 1.0;
    }

    const double term = std::pow(one_minus_base, exponent);
    const double result = 1.0 - term;

    if (result < 0.0) {
        return 0.0;
    }
    if (result > 1.0) {
        return 1.0;
    }
    return result;
}

/**
 * Approximate exceedance probability for V_t > V_FOG
 * under a Gaussian residual model:
 *
 *  V_t ~ N(mu0, sigma0^2)
 *
 *  P(V_t > V_FOG) = 1 - Phi((V_FOG - mu0)/sigma0)
 *
 * where Phi is the standard normal CDF.
 *
 * We implement Phi via an error‑function approximation.
 */
inline double normal_cdf(double x)
{
    // Abramowitz and Stegun approximation for Phi(x).
    const double a1 = 0.254829592;
    const double a2 = -0.284496736;
    const double a3 = 1.421413741;
    const double a4 = -1.453152027;
    const double a5 = 1.061405429;
    const double p = 0.3275911;

    const double sign = x < 0.0 ? -1.0 : 1.0;
    const double abs_x = std::fabs(x) / std::sqrt(2.0);

    const double t = 1.0 / (1.0 + p * abs_x);
    const double y =
        1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-abs_x * abs_x);

    const double erf_val = sign * y;
    return 0.5 * (1.0 + erf_val);
}

inline double probability_residual_exceed(
    double V_FOG,
    double mu0,
    double sigma0
)
{
    if (sigma0 <= 0.0 || !std::isfinite(sigma0)) {
        throw std::invalid_argument("sigma0 must be > 0 and finite.");
    }
    if (!std::isfinite(V_FOG) || !std::isfinite(mu0)) {
        throw std::invalid_argument("V_FOG and mu0 must be finite.");
    }

    const double z = (V_FOG - mu0) / sigma0;
    const double cdf_val = normal_cdf(z);
    const double result = 1.0 - cdf_val;

    if (result < 0.0) {
        return 0.0;
    }
    if (result > 1.0) {
        return 1.0;
    }
    return result;
}

/**
 * Approximate false‑positive probability:
 *
 *   P_FP(T) ≈ P(A_T) * P(B_T)
 *
 * where:
 *   A_T: at least one dropout run of length >= L in window T.
 *   B_T: V_t > V_FOG under H0 (no unmodeled media).
 */
inline double false_positive_probability(
    std::size_t T,
    std::size_t L,
    double p_d,
    double V_FOG,
    double mu0,
    double sigma0
)
{
    const double p_dropout = probability_dropout_run(T, L, p_d);
    const double p_exceed = probability_residual_exceed(V_FOG, mu0, sigma0);

    const double result = p_dropout * p_exceed;
    if (result < 0.0) {
        return 0.0;
    }
    if (result > 1.0) {
        return 1.0;
    }
    return result;
}

/**
 * Predicate: unmodeled_media_detected
 *
 * Returns true if:
 *  - there exists a dropout run of length >= L ending at or before t,
 *  - and V_t > V_FOG.
 *
 * This function assumes the caller has already identified
 * dropout runs in the indicator sequence D (1 = missing, 0 = present).
 */
inline bool unmodeled_media_detected(
    std::size_t t_index,
    const std::vector<int> &dropout_indicator,
    const std::vector<double> &V_cec_series,
    std::size_t L,
    double V_FOG
)
{
    if (V_cec_series.size() != dropout_indicator.size()) {
        throw std::invalid_argument(
            "V_cec_series and dropout_indicator must have same length."
        );
    }
    if (L == 0) {
        throw std::invalid_argument("Dropout length L must be > 0.");
    }
    if (t_index >= dropout_indicator.size()) {
        throw std::out_of_range("t_index out of range.");
    }
    if (!std::isfinite(V_FOG)) {
        throw std::invalid_argument("V_FOG must be finite.");
    }

    // Condition 2: residual exceeds threshold.
    const double V_t = V_cec_series[t_index];
    if (!std::isfinite(V_t)) {
        return false;
    }
    const bool residual_high = (V_t > V_FOG);
    if (!residual_high) {
        return false;
    }

    // Condition 1: exists dropout run of length >= L ending at or before t_index.
    const std::size_t n = dropout_indicator.size();
    if (n < L) {
        return false;
    }

    const std::size_t max_start =
        t_index + 1 < L ? 0 : t_index + 1 - L;

    bool has_run = false;
    for (std::size_t start = 0; start <= max_start; ++start) {
        bool all_missing = true;
        for (std::size_t k = 0; k < L; ++k) {
            const std::size_t idx = start + k;
            if (idx >= n) {
                all_missing = false;
                break;
            }
            if (dropout_indicator[idx] != 1) {
                all_missing = false;
                break;
            }
        }
        if (all_missing) {
            has_run = true;
            break;
        }
    }

    return has_run && residual_high;
}
