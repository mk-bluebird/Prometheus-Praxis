// File: cpp/ecosafety_core_v2/pfas_electrode_kalman_gain.hpp
// Repo path: cpp/ecosafety_core_v2/pfas_electrode_kalman_gain.hpp
//
// Purpose:
//   Non-actuating C++ helper for PFAS electrode Kalman gain schedule:
//     - Adjust scalar concentration gain K_k based on knowledge factor K.
//     - Ensure gains stay within convergence-safe bounds.
//   This is consistent with standard Kalman filter stability results,
//   avoiding too small gains (no learning) and too large gains (instability).

#ifndef ECOSAFETY_CORE_V2_PFAS_ELECTRODE_KALMAN_GAIN_HPP
#define ECOSAFETY_CORE_V2_PFAS_ELECTRODE_KALMAN_GAIN_HPP

#include <stdexcept>

namespace ecosafety_core_v2 {

struct KalmanGainParams {
    double K_min;  // minimum effective gain
    double K_max;  // maximum effective gain
};

// Map knowledge factor K ∈ [0,1] to an effective scalar gain K_gain_eff ∈ [K_min,K_max].
// Example schedule:
//   - When K is low (uncertain), we use higher gain → learn faster.
//   - When K is high (confident), we reduce gain → smoother updates.
inline double compute_kalman_gain(double K_knowledge, const KalmanGainParams& params) {
    if (K_knowledge < 0.0 || K_knowledge > 1.0) {
        throw std::invalid_argument("K_knowledge must be in [0,1]");
    }
    if (params.K_min < 0.0 || params.K_max > 1.0 || params.K_min >= params.K_max) {
        throw std::invalid_argument("Invalid KalmanGainParams bounds");
    }

    // Invert mapping: lower K → higher gain.
    // K=0 → K_gain_eff = K_max; K=1 → K_gain_eff = K_min.
    double K_gain_eff = params.K_min + (1.0 - K_knowledge) * (params.K_max - params.K_min);

    // Clamp for safety (redundant due to construction).
    if (K_gain_eff < params.K_min) K_gain_eff = params.K_min;
    if (K_gain_eff > params.K_max) K_gain_eff = params.K_max;

    return K_gain_eff;
}

// Apply scalar gain to a 1D concentration estimate (simplified update).
// Measurement update:
//   x_hat_k = x_hat_k_prev + K_gain_eff * (z_k - H x_hat_k_prev).
inline double kalman_concentration_update(double x_hat_prev,
                                          double z_meas,
                                          double H_sens,
                                          double K_gain_eff)
{
    // Check gain bounds for stability.
    if (K_gain_eff < 0.0 || K_gain_eff > 1.0) {
        throw std::invalid_argument("K_gain_eff must be in [0,1]");
    }
    // Simple scalar innovation.
    double innovation = z_meas - H_sens * x_hat_prev;
    double x_hat_k = x_hat_prev + K_gain_eff * innovation;
    if (x_hat_k < 0.0) x_hat_k = 0.0; // PFAS concentration cannot be negative.
    return x_hat_k;
}

} // namespace ecosafety_core_v2

#endif // ECOSAFETY_CORE_V2_PFAS_ELECTRODE_KALMAN_GAIN_HPP
