// filename: ecorestorationshard/ecosafety_core_v2/cpp/ai_ker_regret_30day.hpp
// destination: ecorestorationshard/ecosafety_core_v2/cpp/ai_ker_regret_30day.hpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
//
// Purpose:
//   Non-actuating helper to compute per-day KER regret and 30-day aggregates
//   for AI agents, consistent with AIKERRegret30Day2026v1 and the SQL shard.[4]
//   This header is intended for CI/replay harnesses, not runtime actuation.

#ifndef ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_AI_KER_REGRET_30DAY_HPP
#define ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_AI_KER_REGRET_30DAY_HPP

#include <vector>
#include <stdexcept>

namespace ecosafety_core_v2 {

struct KerTargets {
    double k_target;
    double e_target;
    double r_target;
};

struct KerSnapshot {
    double k_actual;
    double e_actual;
    double r_actual;
};

inline double compute_daily_ker_regret(const KerSnapshot& actual,
                                       const KerTargets& target) {
    if (actual.k_actual < 0.0 || actual.k_actual > 1.0 ||
        actual.e_actual < 0.0 || actual.e_actual > 1.0 ||
        actual.r_actual < 0.0 || actual.r_actual > 1.0 ||
        target.k_target < 0.0 || target.k_target > 1.0 ||
        target.e_target < 0.0 || target.e_target > 1.0 ||
        target.r_target < 0.0 || target.r_target > 1.0) {
        throw std::invalid_argument("KER values must be in [0,1]");
    }

    const double k_reg = (target.k_target > actual.k_actual)
        ? (target.k_target - actual.k_actual)
        : 0.0;
    const double e_reg = (target.e_target > actual.e_actual)
        ? (target.e_target - actual.e_actual)
        : 0.0;
    const double r_reg = (actual.r_actual > target.r_target)
        ? (actual.r_actual - target.r_target)
        : 0.0;

    return k_reg + e_reg + r_reg;
}

inline double compute_30day_regret_sum(const std::vector<double>& daily_regrets) {
    if (daily_regrets.size() != 30U) {
        throw std::invalid_argument("Expected 30 days of regret values");
    }
    double sum = 0.0;
    for (double r : daily_regrets) {
        if (r < 0.0) {
            throw std::invalid_argument("Daily regret must be non-negative");
        }
        sum += r;
    }
    return sum;
}

inline double compute_30day_regret_avg(const std::vector<double>& daily_regrets) {
    const double sum = compute_30day_regret_sum(daily_regrets);
    return sum / 30.0;
}

} // namespace ecosafety_core_v2

#endif // ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_AI_KER_REGRET_30DAY_HPP
