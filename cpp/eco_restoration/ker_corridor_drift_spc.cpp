// File: cpp/eco_restoration/ker_corridor_drift_spc.cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct KerDailyMetric {
    std::string day;
    double knowledge_factor{};
    double eco_impact_value{};
    double risk{};
};

struct SpcPoint {
    std::string day;
    double moving_knowledge{};
    double moving_impact{};
    double moving_risk{};
    bool alert{};
};

std::vector<SpcPoint> detect_ker_corridor_drift(const std::vector<KerDailyMetric>& metrics,
                                                double baseline_k, double baseline_e,
                                                double baseline_r, double standard_deviation) {
    if (metrics.size() < 7 || standard_deviation <= 0.0)
        throw std::invalid_argument("seven daily observations and positive deviation required");

    constexpr std::size_t window = 7;
    constexpr double control_width = 3.0;
    std::vector<SpcPoint> results;
    for (std::size_t end = window - 1; end < metrics.size(); ++end) {
        double k = 0.0, e = 0.0, r = 0.0;
        for (std::size_t i = end + 1 - window; i <= end; ++i) {
            const auto& sample = metrics[i];
            if (sample.knowledge_factor < 0.0 || sample.knowledge_factor > 1.0 ||
                sample.eco_impact_value < 0.0 || sample.eco_impact_value > 1.0 ||
                sample.risk < 0.0 || sample.risk > 1.0)
                throw std::invalid_argument("invalid K/E/R sample");
            k += sample.knowledge_factor;
            e += sample.eco_impact_value;
            r += sample.risk;
        }
        k /= window; e /= window; r /= window;
        const double limit = control_width * standard_deviation / std::sqrt(static_cast<double>(window));
        const bool alert = k < baseline_k - limit || e < baseline_e - limit || r > baseline_r + limit;
        results.push_back({metrics[end].day, k, e, r, alert});
    }
    return results;
}

}  // namespace eco_restoration
