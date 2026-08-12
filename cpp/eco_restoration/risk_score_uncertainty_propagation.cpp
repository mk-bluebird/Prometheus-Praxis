// File: cpp/eco_restoration/risk_score_uncertainty_propagation.cpp
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ppx::eco_restoration {

struct EnergyInputs {
    double power_w{}, duration_s{}, renewable_fraction{}, grid_carbon_g_per_kwh{}, reference_carbon_g{};
};

double r_energy(const EnergyInputs& x) {
    return std::clamp(
        x.power_w * x.duration_s * (1.0 - x.renewable_fraction) *
        x.grid_carbon_g_per_kwh / (3'600'000.0 * x.reference_carbon_g),
        0.0, 1.0);
}

double independent_variance(std::span<const double> jacobian, std::span<const double> sigma) {
    if (jacobian.size() != sigma.size()) throw std::invalid_argument("Jacobian dimensions differ");
    double result = 0.0;
    for (std::size_t i = 0; i < jacobian.size(); ++i) result += jacobian[i] * jacobian[i] * sigma[i] * sigma[i];
    return result;
}

double r_energy_variance(const EnergyInputs& x, std::span<const double> sigma) {
    if (sigma.size() != 4 || x.reference_carbon_g <= 0.0) throw std::invalid_argument("invalid energy uncertainty");
    const double denominator = 3'600'000.0 * x.reference_carbon_g;
    const double raw = x.power_w * x.duration_s * (1.0 - x.renewable_fraction) *
                       x.grid_carbon_g_per_kwh / denominator;
    if (raw <= 0.0 || raw >= 1.0) return 0.0;
    const std::array<double, 4> jacobian{
        x.duration_s * (1.0 - x.renewable_fraction) * x.grid_carbon_g_per_kwh / denominator,
        x.power_w * (1.0 - x.renewable_fraction) * x.grid_carbon_g_per_kwh / denominator,
        -x.power_w * x.duration_s * x.grid_carbon_g_per_kwh / denominator,
        x.power_w * x.duration_s * (1.0 - x.renewable_fraction) / denominator
    };
    return independent_variance(jacobian, sigma);
}

struct HeatInputs {
    std::vector<double> temperatures_c;
    double base_c{}, range_c{}, variability_weight_per_c{};
};

double r_heat(const HeatInputs& x) {
    if (x.temperatures_c.empty() || x.range_c <= 0.0) throw std::invalid_argument("invalid heat inputs");
    const double mean = std::accumulate(x.temperatures_c.begin(), x.temperatures_c.end(), 0.0) /
                        static_cast<double>(x.temperatures_c.size());
    double variance = 0.0;
    for (double t : x.temperatures_c) variance += (t - mean) * (t - mean);
    const double sd = std::sqrt(variance / static_cast<double>(x.temperatures_c.size()));
    const double maximum = *std::max_element(x.temperatures_c.begin(), x.temperatures_c.end());
    return std::clamp((maximum - x.base_c) / x.range_c + x.variability_weight_per_c * sd, 0.0, 1.0);
}

double monte_carlo_energy_variance(EnergyInputs mean, std::span<const double> sigma,
                                   std::size_t trials, std::uint64_t seed = 20260811) {
    if (sigma.size() != 4 || trials < 2) throw std::invalid_argument("invalid Monte Carlo configuration");
    std::mt19937_64 engine(seed);
    std::normal_distribution<double> p(mean.power_w, sigma[0]);
    std::normal_distribution<double> t(mean.duration_s, sigma[1]);
    std::normal_distribution<double> f(mean.renewable_fraction, sigma[2]);
    std::normal_distribution<double> c(mean.grid_carbon_g_per_kwh, sigma[3]);

    double total = 0.0, square_total = 0.0;
    for (std::size_t i = 0; i < trials; ++i) {
        const EnergyInputs sample{
            std::max(0.0, p(engine)), std::max(0.0, t(engine)),
            std::clamp(f(engine), 0.0, 1.0), std::max(0.0, c(engine)),
            mean.reference_carbon_g
        };
        const double risk = r_energy(sample);
        total += risk;
        square_total += risk * risk;
    }
    return (square_total - total * total / static_cast<double>(trials)) /
           static_cast<double>(trials - 1);
}

}  // namespace ppx::eco_restoration
