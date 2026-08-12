// File: cpp/eco_restoration/eco_workload_admission.cpp
// Evidence context: [51]

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

constexpr double kEpsilon = 1e-12;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

struct HexAnchor {
    std::uint8_t level{};
    std::uint32_t row{};
    std::uint32_t column{};

    std::optional<std::uint64_t> pack64() const {
        constexpr std::uint32_t kCoordinateLimit = (1U << 30U) - 1U;
        if (level > 15U || row > kCoordinateLimit || column > kCoordinateLimit) {
            return std::nullopt;
        }
        return (static_cast<std::uint64_t>(level) << 60U) |
               (static_cast<std::uint64_t>(row) << 30U) |
               static_cast<std::uint64_t>(column);
    }

    static HexAnchor unpack64(std::uint64_t value) {
        return HexAnchor{
            static_cast<std::uint8_t>((value >> 60U) & 0x0FU),
            static_cast<std::uint32_t>((value >> 30U) & 0x3FFFFFFFU),
            static_cast<std::uint32_t>(value & 0x3FFFFFFFU)
        };
    }
};

struct WorkloadInput {
    double accelerator_seconds{};
    double average_power_w{};
    double grid_carbon_g_per_kwh{};
    double renewable_fraction{};
    double verified_ecological_benefit_g{};
    double ai_confidence{};
    std::vector<double> sensor_reliabilities{};
    std::vector<double> sensor_weights{};
    double energy_reference_g{};
    double heat_c{};
    double heat_baseline_c{};
    double heat_range_c{};
    double heat_standard_deviation_c{};
    double heat_uncertainty_gain{};
    double biodiversity_richness{};
    double biodiversity_connectivity{};
    double biodiversity_fragmentation{};
    std::array<double, 3> biodiversity_weights{};
    double sediment_concentration{};
    double sediment_reference_concentration{};
    double shields_parameter{};
    double critical_shields_parameter{};
    double water_risk{};
    double lyapunov_delta{};
    double barrier_value{};
    double barrier_gamma{};
    double power_cap_w{};
    double thermal_cap_c{};
};

struct Evaluation {
    double knowledge_factor{};
    double eco_impact_value{};
    double combined_risk{};
    double energy_kwh{};
    double carbon_g{};
    bool carbon_negative{};
    std::string decision;
    std::string reason;
};

double weighted_geometric_knowledge(
    double confidence,
    const std::vector<double>& reliability,
    const std::vector<double>& weights) {

    if (reliability.size() != weights.size() || confidence <= 0.0 || confidence > 1.0) {
        throw std::invalid_argument("invalid confidence or reliability weights");
    }

    double weighted_log_sum = std::log(confidence);
    double total_weight = 1.0;
    for (std::size_t i = 0; i < reliability.size(); ++i) {
        if (reliability[i] <= 0.0 || reliability[i] > 1.0 || weights[i] < 0.0) {
            throw std::invalid_argument("reliability must be in (0,1] and weight must be nonnegative");
        }
        weighted_log_sum += weights[i] * std::log(reliability[i]);
        total_weight += weights[i];
    }
    return std::exp(weighted_log_sum / total_weight);
}

double biodiversity_risk(const WorkloadInput& in) {
    const double weight_sum = in.biodiversity_weights[0] +
                              in.biodiversity_weights[1] +
                              in.biodiversity_weights[2];
    if (std::abs(weight_sum - 1.0) > 1e-9) {
        throw std::invalid_argument("biodiversity weights must sum to one");
    }
    return clamp01(
        in.biodiversity_weights[0] * (1.0 - clamp01(in.biodiversity_richness)) +
        in.biodiversity_weights[1] * (1.0 - clamp01(in.biodiversity_connectivity)) +
        in.biodiversity_weights[2] * clamp01(in.biodiversity_fragmentation));
}

Evaluation evaluate(const WorkloadInput& in) {
    if (in.accelerator_seconds < 0.0 || in.average_power_w < 0.0 ||
        in.grid_carbon_g_per_kwh < 0.0 || in.energy_reference_g <= 0.0 ||
        in.heat_range_c <= 0.0 || in.sediment_reference_concentration < 0.0 ||
        in.critical_shields_parameter <= 0.0) {
        throw std::invalid_argument("invalid physical input");
    }

    const double energy_kwh = (in.accelerator_seconds * in.average_power_w) / 3'600'000.0;
    const double carbon_g = energy_kwh * (1.0 - clamp01(in.renewable_fraction)) *
                            in.grid_carbon_g_per_kwh;
    const double energy_risk = clamp01(carbon_g / in.energy_reference_g);
    const double heat_risk = clamp01(
        (in.heat_c - in.heat_baseline_c) / in.heat_range_c +
        in.heat_uncertainty_gain * std::max(0.0, in.heat_standard_deviation_c));
    const double sediment_fraction = in.sediment_concentration /
        (in.sediment_concentration + in.sediment_reference_concentration + kEpsilon);
    const double sediment_risk = clamp01(
        sediment_fraction * clamp01(1.0 - in.shields_parameter / in.critical_shields_parameter));
    const double bio_risk = biodiversity_risk(in);
    const double combined_risk = std::max(
        {energy_risk, heat_risk, sediment_risk, bio_risk, clamp01(in.water_risk)});

    const double knowledge = weighted_geometric_knowledge(
        in.ai_confidence, in.sensor_reliabilities, in.sensor_weights);
    const double ecological_evidence = clamp01(
        in.verified_ecological_benefit_g /
        (in.verified_ecological_benefit_g + carbon_g + kEpsilon));
    const double impact = clamp01(knowledge * ecological_evidence * (1.0 - combined_risk));

    const bool barrier_safe = in.barrier_value >=
        (1.0 - clamp01(in.barrier_gamma)) * in.barrier_value +
        std::max(0.0, in.lyapunov_delta);
    const bool carbon_negative = in.verified_ecological_benefit_g > carbon_g;

    Evaluation result{
        knowledge, impact, combined_risk, energy_kwh, carbon_g, carbon_negative, "HALT", ""
    };

    if (in.average_power_w > in.power_cap_w || in.heat_c > in.thermal_cap_c) {
        result.reason = "power or thermal cap exceeded";
    } else if (!carbon_negative) {
        result.reason = "verified ecological benefit does not exceed attributable carbon";
    } else if (!barrier_safe || in.lyapunov_delta > 0.02) {
        result.reason = "ecological stability corridor is not satisfied";
    } else if (combined_risk > 0.35 || impact < 0.55) {
        result.decision = "DERATE";
        result.reason = "admit only with reduced resource allocation";
    } else {
        result.decision = "PROCEED";
        result.reason = "carbon-negative workload satisfies ecological corridor";
    }
    return result;
}

}  // namespace eco_restoration

int main() {
    using namespace eco_restoration;

    WorkloadInput input{
        1800.0, 420.0, 420.0, 0.72, 180.0, 0.93,
        {0.96, 0.89, 0.91}, {1.0, 1.0, 1.0},
        300.0, 35.0, 30.0, 15.0, 0.03,
        0.78, 0.70, 0.16, {0.4, 0.35, 0.25},
        5.0, 20.0, 0.08, 0.12, 0.18,
        0.008, 0.40, 0.10, 500.0, 42.0
    };

    try {
        const Evaluation result = evaluate(input);
        std::cout << std::fixed << std::setprecision(6)
                  << "{\"decision\":\"" << result.decision
                  << "\",\"reason\":\"" << result.reason
                  << "\",\"knowledge_factor\":" << result.knowledge_factor
                  << ",\"eco_impact_value\":" << result.eco_impact_value
                  << ",\"combined_risk\":" << result.combined_risk
                  << ",\"energy_kwh\":" << result.energy_kwh
                  << ",\"carbon_g\":" << result.carbon_g
                  << ",\"carbon_negative\":" << (result.carbon_negative ? "true" : "false")
                  << "}\n";
    } catch (const std::exception& error) {
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
