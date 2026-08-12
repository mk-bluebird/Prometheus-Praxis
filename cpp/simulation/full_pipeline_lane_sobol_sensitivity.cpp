// File: cpp/simulation/full_pipeline_lane_sobol_sensitivity.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

enum class LaneDecision { Halt, Derate, Proceed };

struct UncertaintyRange {
    std::string name;
    double minimum{};
    double maximum{};
};

struct SobolIndex {
    std::string variable;
    double first_order{};
    double total_order{};
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

LaneDecision evaluate_lane(const std::vector<double>& x) {
    const double air_temperature_c = x[0];
    const double water_quality = clamp01(x[1]);
    const double sensor_noise = std::max(0.0, x[2]);
    const double grid_carbon_g_kwh = std::max(0.0, x[3]);
    const double verified_benefit_g = std::max(0.0, x[4]);
    const double model_risk_bias = x[5];
    const double lyapunov_delta = std::max(0.0, x[6]);

    const double heat_risk = clamp01((air_temperature_c - 30.0) / 15.0 + sensor_noise);
    const double water_risk = clamp01(1.0 - water_quality + sensor_noise);
    const double energy_risk = clamp01(grid_carbon_g_kwh / 600.0);
    const double risk = clamp01(std::max({heat_risk, water_risk, energy_risk}) + model_risk_bias);
    const double knowledge = clamp01(1.0 - sensor_noise);
    const double eco_value = knowledge * (verified_benefit_g / (verified_benefit_g + grid_carbon_g_kwh + 1.0)) *
                             (1.0 - risk);

    if (lyapunov_delta > 0.02 || verified_benefit_g <= grid_carbon_g_kwh || risk > 0.70)
        return LaneDecision::Halt;
    if (risk > 0.35 || eco_value < 0.55) return LaneDecision::Derate;
    return LaneDecision::Proceed;
}

double encoded_action(const std::vector<double>& x) {
    switch (evaluate_lane(x)) {
        case LaneDecision::Halt: return 0.0;
        case LaneDecision::Derate: return 0.5;
        case LaneDecision::Proceed: return 1.0;
    }
    return 0.0;
}

std::vector<SobolIndex> lane_decision_sobol(const std::vector<UncertaintyRange>& ranges,
                                            std::size_t samples = 4096) {
    if (ranges.empty() || samples < 1000) throw std::invalid_argument("insufficient Sobol design");
    for (const auto& range : ranges)
        if (range.name.empty() || range.maximum <= range.minimum)
            throw std::invalid_argument("invalid uncertainty range");

    std::mt19937_64 generator(0x5EED2026ULL);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    const std::size_t dimensions = ranges.size();
    std::vector<std::vector<double>> a(samples, std::vector<double>(dimensions));
    std::vector<std::vector<double>> b(samples, std::vector<double>(dimensions));
    std::vector<double> ya(samples), yb(samples);

    double mean = 0.0;
    for (std::size_t row = 0; row < samples; ++row) {
        for (std::size_t col = 0; col < dimensions; ++col) {
            const auto& range = ranges[col];
            a[row][col] = range.minimum + unit(generator) * (range.maximum - range.minimum);
            b[row][col] = range.minimum + unit(generator) * (range.maximum - range.minimum);
        }
        ya[row] = encoded_action(a[row]);
        yb[row] = encoded_action(b[row]);
        mean += ya[row] + yb[row];
    }
    mean /= static_cast<double>(2 * samples);

    double variance = 0.0;
    for (std::size_t row = 0; row < samples; ++row) {
        variance += std::pow(ya[row] - mean, 2) + std::pow(yb[row] - mean, 2);
    }
    variance /= static_cast<double>(2 * samples - 1);
    if (variance < 1e-12) throw std::runtime_error("lane action has no measurable variation");

    std::vector<SobolIndex> result;
    for (std::size_t variable = 0; variable < dimensions; ++variable) {
        double first = 0.0, total = 0.0;
        for (std::size_t row = 0; row < samples; ++row) {
            auto hybrid = a[row];
            hybrid[variable] = b[row][variable];
            const double y_hybrid = encoded_action(hybrid);
            first += yb[row] * (y_hybrid - ya[row]);
            total += std::pow(ya[row] - y_hybrid, 2);
        }
        result.push_back({ranges[variable].name,
                          std::clamp(first / (samples * variance), 0.0, 1.0),
                          std::clamp(total / (2.0 * samples * variance), 0.0, 1.0)});
    }
    return result;
}

}  // namespace eco_restoration
