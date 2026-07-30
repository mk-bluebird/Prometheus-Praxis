// filename: ecorestorationshard/ecosafety_core_v2/cpp/carbon_negative_check.hpp
// destination: ecorestorationshard/ecosafety_core_v2/cpp/carbon_negative_check.hpp
// repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
//
// Purpose:
//   Non-actuating C++ header to:
//     - Compute per-interval energy and carbon offset differences.
//     - Check the inequality ΔC >= 1.2 * ΔE.
//     - Report violations in a way compatible with ecosafety_core_v2
//       and the Phoenix hex registry.
//   This is intended for CI or runtime monitoring, not for actuation.

#ifndef ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_CARBON_NEGATIVE_CHECK_HPP
#define ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_CARBON_NEGATIVE_CHECK_HPP

#include <vector>
#include <string>
#include <stdexcept>

namespace ecosafety_core_v2 {

struct CarbonDailyRow {
    std::string yyyymmdd;
    double      energy_in_J;
    double      carbon_offset_J;
};

struct CarbonIntervalResult {
    std::string node_id;
    std::string yyyymmdd_start;
    std::string yyyymmdd_end;
    double      delta_energy_J;
    double      delta_offset_J;
    bool        inequality_holds;
};

inline std::vector<CarbonIntervalResult>
compute_carbon_intervals(const std::string& node_id,
                         const std::vector<CarbonDailyRow>& daily_rows)
{
    if (daily_rows.size() < 2) {
        throw std::invalid_argument("Need at least 2 daily rows to form intervals");
    }

    std::vector<CarbonIntervalResult> intervals;
    intervals.reserve(daily_rows.size() - 1);

    for (std::size_t i = 0; i + 1 < daily_rows.size(); ++i) {
        const auto& d0 = daily_rows[i];
        const auto& d1 = daily_rows[i + 1];

        if (d1.energy_in_J < d0.energy_in_J ||
            d1.carbon_offset_J < d0.carbon_offset_J) {
            // Non-monotone telemetry; treat as potential data issue.
            // Still compute deltas but note that negative deltas may occur.
        }

        const double deltaE = d1.energy_in_J - d0.energy_in_J;
        const double deltaC = d1.carbon_offset_J - d0.carbon_offset_J;

        CarbonIntervalResult res;
        res.node_id         = node_id;
        res.yyyymmdd_start  = d0.yyyymmdd;
        res.yyyymmdd_end    = d1.yyyymmdd;
        res.delta_energy_J  = deltaE;
        res.delta_offset_J  = deltaC;
        res.inequality_holds = (deltaC + 1e-9) >= 1.2 * deltaE; // small epsilon

        intervals.push_back(res);
    }

    return intervals;
}

inline bool check_horizon_carbon_negative(const std::vector<CarbonDailyRow>& daily_rows) {
    if (daily_rows.empty()) {
        throw std::invalid_argument("No daily rows for horizon check");
    }

    const double total_energy_J =
        daily_rows.back().energy_in_J - daily_rows.front().energy_in_J;
    const double total_offset_J =
        daily_rows.back().carbon_offset_J - daily_rows.front().carbon_offset_J;

    return (total_offset_J + 1e-9) >= 1.2 * total_energy_J;
}

} // namespace ecosafety_core_v2

#endif // ECORESTORATIONSHARD_ECOSAFETY_CORE_V2_CARBON_NEGATIVE_CHECK_HPP
