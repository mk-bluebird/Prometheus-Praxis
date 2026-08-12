// File: cpp/eco_restoration/eco_workload_admission.cpp

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

constexpr double kEpsilon = 1e-12;

[[nodiscard]] double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

struct HexAnchor {
    std::uint8_t level{};
    std::uint32_t row{};
    std::uint32_t column{};

    [[nodiscard]] std::optional<std::uint64_t> pack64() const {
        constexpr std::uint32_t coordinate_limit = (1U << 30U) - 1U;
        if (level > 15U || row > coordinate_limit || column > coordinate_limit) {
            return std::nullopt;
        }
        return (static_cast<std::uint64_t>(level) << 60U) |
               (static_cast<std::uint64_t>(row) << 30U) |
               static_cast<std::uint64_t>(column);
    }

    [[nodiscard]] static HexAnchor unpack64(std::uint64_t value) {
        return {
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

void validate_finite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

void validate_input(const WorkloadInput& input) {
    const std::array<double, 22> values{
        input.accelerator_seconds,
        input.average_power_w,
        input.grid_carbon_g_per_kwh,
        input.renewable_fraction,
        input.verified_ecological_benefit_g,
        input.ai_confidence,
        input.energy_reference_g,
        input.heat_c,
        input.heat_baseline_c,
        input.heat_range_c,
        input.heat_standard_deviation_c,
        input.heat_uncertainty_gain,
        input.biodiversity_richness,
        input.biodiversity_connectivity,
        input.biodiversity_fragmentation,
        input.sediment_concentration,
        input.sediment_reference_concentration,
        input.shields_parameter,
        input.critical_shields_parameter,
        input.water_risk,
        input.lyapunov_delta,
        input.barrier_value
    };

    for (double value : values) {
        validate_finite(value, "workload input");
    }

    validate_finite(input.barrier_gamma, "barrier gamma");
    validate_finite(input.power_cap_w, "power cap");
    validate_finite(input.thermal_cap_c, "thermal cap");

    if (input.accelerator_seconds < 0.0 ||
        input.average_power_w < 0.0 ||
        input.grid_carbon_g_per_kwh < 0.0 ||
        input.verified_ecological_benefit_g < 0.0 ||
        input.energy_reference_g <= 0.0 ||
        input.heat_range_c <= 0.0 ||
        input.heat_standard_deviation_c < 0.0 ||
        input.heat_uncertainty_gain < 0.0 ||
        input.sediment_concentration < 0.0 ||
        input.sediment_reference_concentration < 0.0 ||
        input.critical_shields_parameter <= 0.0 ||
        input.lyapunov_delta < 0.0 ||
        input.barrier_value < 0.0 ||
        input.power_cap_w < 0.0 ||
        input.thermal_cap_c < 0.0 ||
        input.barrier_gamma < 0.0 ||
        input.barrier_gamma > 1.0 ||
        input.ai_confidence <= 0.0 ||
        input.ai_confidence > 1.0 ||
        input.sensor_reliabilities.size() != input.sensor_weights.size()) {
        throw std::invalid_argument("invalid ecological workload input");
    }
}

[[nodiscard]] double weighted_geometric_knowledge(
    double confidence,
    const std::vector<double>& reliability,
    const std::vector<double>& weights) {

    double weighted_log_sum = std::log(confidence);
    double total_weight = 1.0;

    for (std::size_t i = 0; i < reliability.size(); ++i) {
        validate_finite(reliability[i], "sensor reliability");
        validate_finite(weights[i], "sensor weight");
        if (reliability[i] <= 0.0 || reliability[i] > 1.0 || weights[i] < 0.0) {
            throw std::invalid_argument("invalid sensor reliability or weight");
        }
        weighted_log_sum += weights[i] * std::log(reliability[i]);
        total_weight += weights[i];
    }

    return std::exp(weighted_log_sum / total_weight);
}

[[nodiscard]] double biodiversity_risk(const WorkloadInput& input) {
    const double weight_sum = input.biodiversity_weights[0] +
                              input.biodiversity_weights[1] +
                              input.biodiversity_weights[2];

    for (double weight : input.biodiversity_weights) {
        validate_finite(weight, "biodiversity weight");
        if (weight < 0.0) {
            throw std::invalid_argument("biodiversity weights must be nonnegative");
        }
    }

    if (std::abs(weight_sum - 1.0) > 1e-9) {
        throw std::invalid_argument("biodiversity weights must sum to one");
    }

    return clamp01(
        input.biodiversity_weights[0] * (1.0 - clamp01(input.biodiversity_richness)) +
        input.biodiversity_weights[1] * (1.0 - clamp01(input.biodiversity_connectivity)) +
        input.biodiversity_weights[2] * clamp01(input.biodiversity_fragmentation));
}

[[nodiscard]] Evaluation evaluate(const WorkloadInput& input) {
    validate_input(input);

    const double energy_kwh =
        input.accelerator_seconds * input.average_power_w / 3'600'000.0;
    const double carbon_g =
        energy_kwh * (1.0 - clamp01(input.renewable_fraction)) * input.grid_carbon_g_per_kwh;
    const double energy_risk = clamp01(carbon_g / input.energy_reference_g);
    const double heat_risk = clamp01(
        (input.heat_c - input.heat_baseline_c) / input.heat_range_c +
        input.heat_uncertainty_gain * input.heat_standard_deviation_c);
    const double sediment_fraction = input.sediment_concentration /
        (input.sediment_concentration + input.sediment_reference_concentration + kEpsilon);
    const double sediment_risk = clamp01(
        sediment_fraction *
        clamp01(1.0 - input.shields_parameter / input.critical_shields_parameter));
    const double combined_risk = std::max({
        energy_risk,
        heat_risk,
        sediment_risk,
        biodiversity_risk(input),
        clamp01(input.water_risk)
    });

    const double knowledge = weighted_geometric_knowledge(
        input.ai_confidence,
        input.sensor_reliabilities,
        input.sensor_weights);
    const double ecological_evidence = clamp01(
        input.verified_ecological_benefit_g /
        (input.verified_ecological_benefit_g + carbon_g + kEpsilon));
    const double eco_impact = clamp01(
        knowledge * ecological_evidence * (1.0 - combined_risk));
    const double required_barrier =
        (1.0 - input.barrier_gamma) * input.barrier_value +
        input.lyapunov_delta;
    const bool barrier_safe = input.barrier_value >= required_barrier;
    const bool carbon_negative = input.verified_ecological_benefit_g > carbon_g;

    Evaluation result{
        knowledge,
        eco_impact,
        combined_risk,
        energy_kwh,
        carbon_g,
        carbon_negative,
        "HALT",
        "ecological admission requirements are not satisfied"
    };

    if (input.average_power_w > input.power_cap_w || input.heat_c > input.thermal_cap_c) {
        result.reason = "power or thermal cap exceeded";
    } else if (!carbon_negative) {
        result.reason = "verified ecological benefit does not exceed attributable carbon";
    } else if (!barrier_safe || input.lyapunov_delta > 0.02) {
        result.reason = "ecological stability corridor is not satisfied";
    } else if (combined_risk > 0.35 || eco_impact < 0.55) {
        result.decision = "DERATE";
        result.reason = "admit only with reduced resource allocation";
    } else {
        result.decision = "PROCEED";
        result.reason = "carbon-negative workload satisfies ecological corridor";
    }

    return result;
}

[[nodiscard]] WorkloadInput default_workload_input() {
    WorkloadInput input{};
    input.accelerator_seconds = 1800.0;
    input.average_power_w = 420.0;
    input.grid_carbon_g_per_kwh = 420.0;
    input.renewable_fraction = 0.72;
    input.verified_ecological_benefit_g = 180.0;
    input.ai_confidence = 0.93;
    input.sensor_reliabilities = {0.96, 0.89, 0.91};
    input.sensor_weights = {1.0, 1.0, 1.0};
    input.energy_reference_g = 300.0;
    input.heat_c = 35.0;
    input.heat_baseline_c = 30.0;
    input.heat_range_c = 15.0;
    input.heat_standard_deviation_c = 0.03;
    input.heat_uncertainty_gain = 0.78;
    input.biodiversity_richness = 0.70;
    input.biodiversity_connectivity = 0.16;
    input.biodiversity_fragmentation = 0.20;
    input.biodiversity_weights = {0.40, 0.35, 0.25};
    input.sediment_concentration = 5.0;
    input.sediment_reference_concentration = 20.0;
    input.shields_parameter = 0.08;
    input.critical_shields_parameter = 0.12;
    input.water_risk = 0.18;
    input.lyapunov_delta = 0.008;
    input.barrier_value = 0.40;
    input.barrier_gamma = 0.10;
    input.power_cap_w = 500.0;
    input.thermal_cap_c = 42.0;
    return input;
}

}  // namespace eco_restoration

int main() {
    using namespace eco_restoration;

    try {
        const Evaluation result = evaluate(default_workload_input());
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
