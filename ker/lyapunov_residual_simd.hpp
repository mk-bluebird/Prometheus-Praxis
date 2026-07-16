#ifndef PROMETHEUS_PRAXIS_LYAPUNOV_RESIDUAL_SIMD_HPP
#define PROMETHEUS_PRAXIS_LYAPUNOV_RESIDUAL_SIMD_HPP

#include <cstddef>

extern "C" {

/// Compute Lyapunov residual V_t = sum_j w_j * r_j^2 using SIMD where possible.
///
/// Inputs:
///   weights  - pointer to array of weights w_j (float)
///   risks    - pointer to array of risk coordinates r_j (float)
///   len      - number of elements (planes)
///
/// Returns:
///   V_t as double.
///
/// Requirements:
///   - weights and risks must be non-null when len > 0.
///   - Elements are assumed finite (no NaN/Inf).
double lyapunov_residual_simd(
    const float* weights,
    const float* risks,
    std::size_t len
);

} // extern "C"

#endif
