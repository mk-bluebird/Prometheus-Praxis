// filename: ecorestorationshard/ecosafety_core_v2/cpp/pfas_delay_guard.hpp
// destination: ecorestorationshard/ecosafety_core_v2/cpp/pfas_delay_guard.hpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
//
// Purpose:
//   Non-actuating helper to check PFAS delay guard:
//   tau_current <= tau_max_bound, where
//   tau_max_bound = epsilon / (w_pfas * (2*alpha_pfas + 4*beta_pfas) * L_pfas).[4][6]
//   This is intended for replay/CI harnesses, not runtime actuation.

#ifndef ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_PFAS_DELAY_GUARD_HPP
#define ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_PFAS_DELAY_GUARD_HPP

#include <stdexcept>

namespace ecosafety_core_v2 {

struct PFASDelayParams {
    int    tau_current;
    double w_pfas;
    double alpha_pfas;
    double beta_pfas;
    double L_pfas;
    double epsilon;
};

inline double compute_tau_max_bound(const PFASDelayParams& p) {
    if (p.w_pfas <= 0.0 || p.alpha_pfas <= 0.0 || p.beta_pfas <= 0.0 || p.L_pfas <= 0.0 || p.epsilon <= 0.0) {
        throw std::invalid_argument("PFASDelayParams: invalid Lyapunov parameters");
    }
    const double denom = p.w_pfas * (2.0 * p.alpha_pfas + 4.0 * p.beta_pfas) * p.L_pfas;
    return p.epsilon / denom;
}

inline bool check_pfas_delay_guard(const PFASDelayParams& p) {
    const double tau_max_bound = compute_tau_max_bound(p);
    return static_cast<double>(p.tau_current) <= tau_max_bound;
}

} // namespace ecosafety_core_v2

#endif // ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_PFAS_DELAY_GUARD_HPP
