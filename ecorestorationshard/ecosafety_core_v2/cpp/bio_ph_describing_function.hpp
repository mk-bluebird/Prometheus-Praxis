// filename: ecorestorationshard/ecosafety_core_v2/cpp/bio_ph_describing_function.hpp
// destination: ecorestorationshard/ecosafety_core_v2/cpp/bio_ph_describing_function.hpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
//
// Purpose:
//   Non-actuating C++ skeleton to:
//     - Evaluate a simplified describing function N(A) for a bio-controller
//       (algae pH regulator) with saturation-like response.
//     - Provide Nyquist-friendly gain estimates for stability analysis.
//   This header does not perform any actuation; it is for analysis/CI.

#ifndef ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_BIO_PH_DESCRIBING_FUNCTION_HPP
#define ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_BIO_PH_DESCRIBING_FUNCTION_HPP

#include <cmath>
#include <stdexcept>

namespace ecosafety_core_v2 {

struct BioPHStaticParams {
    double k_gain;   // small-signal gain
    double e_sat_pos; // positive saturation threshold
    double e_sat_neg; // negative saturation threshold
};

// Simplified describing function for a symmetric saturation-like nonlinearity.
// For input e(t) = A sin(ω t), output u = f(e) saturates beyond e_sat_pos/e_sat_neg.
// Here we approximate N(A) as a real gain that decreases with amplitude A.
inline double describing_function_gain(const BioPHStaticParams& p, double A) {
    if (A < 0.0) {
        throw std::invalid_argument("Amplitude A must be non-negative");
    }
    const double A_sat_pos = std::fabs(p.e_sat_pos);
    const double A_sat_neg = std::fabs(p.e_sat_neg);

    if (A <= A_sat_pos && A <= A_sat_neg) {
        // Within linear range, gain ~ k_gain.
        return p.k_gain;
    }

    // Above saturation, effective gain is reduced.
    // This is a crude approximation: N(A) ~ k_gain * (A_sat / A).
    const double A_sat = std::min(A_sat_pos, A_sat_neg);
    if (A_sat <= 0.0) {
        return 0.0;
    }
    return p.k_gain * (A_sat / A);
}

// Nyquist-friendly check: given plant gain |G(jω)| at relevant ω and DF N(A),
// check if open-loop gain remains below a stability margin.
inline bool nyquist_margin_ok(double plant_gain, double N_real, double margin) {
    // Simple condition: |G(jω) N(A)| <= margin for stability.
    const double loop_gain = plant_gain * N_real;
    return loop_gain <= margin;
}

} // namespace ecosafety_core_v2

#endif // ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_BIO_PH_DESCRIBING_FUNCTION_HPP
