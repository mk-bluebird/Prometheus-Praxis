// File: cpp/simulation/integrated_ecological_uncertainty_pipeline.cpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct EnvironmentalContract {
    std::int64_t observed_unix_s{};
    double storage_m3{};
    double storage_standard_deviation_m3{};
    double precipitation_m3{};
    double precipitation_standard_deviation_m3{};
    double inflow_m3{};
    double inflow_standard_deviation_m3{};
    double outflow_m3{};
    double outflow_standard_deviation_m3{};
    double evapotranspiration_m3{};
    double evapotranspiration_standard_deviation_m3{};
    double seepage_m3{};
    double seepage_standard_deviation_m3{};
    double soil_temperature_c{};
    double soil_temperature_standard_deviation_c{};
    double habitat_connectivity{};
    double fragmentation{};
    double telemetry_quality{};
};

struct PipelineResult {
    double water_risk_mean{};
    double water_risk_p95{};
    double heat_risk_mean{};
    double heat_risk_p95{};
    double biodiversity_risk_mean{};
    double biodiversity_risk_p95{};
    double knowledge_factor{};
    double eco_impact_mean{};
    double eco_impact_p05{};
    std::string action{};
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

void validate_contract(const EnvironmentalContract& input) {
    if (input.observed_unix_s <= 0 || input.storage_m3 < 0.0 ||
        input.precipitation_m3 < 0.0 || input.inflow_m3 < 0.0 ||
        input.outflow_m3 < 0.0 || input.evapotranspiration_m3 < 0.0 ||
        input.seepage_m3 < 0.0 || input.habitat_connectivity < 0.0 ||
        input.habitat_connectivity > 1.0 || input.fragmentation < 0.0 ||
        input.fragmentation > 1.0 || input.telemetry_quality < 0.0 ||
        input.telemetry_quality > 1.0) {
        throw std::invalid_argument("environmental data contract is invalid");
    }

    for (double uncertainty : {
        input.storage_standard_deviation_m3,
        input.precipitation_standard_deviation_m3,
        input.inflow_standard_deviation_m3,
        input.outflow_standard_deviation_m3,
        input.evapotranspiration_standard_deviation_m3,
        input.seepage_standard_deviation_m3,
        input.soil_temperature_standard_deviation_c
    }) {
        if (uncertainty < 0.0 || !std::isfinite(uncertainty)) {
            throw std::invalid_argument("uncertainty values must be finite and nonnegative");
        }
    }
}

double percentile(std::vector<double> values, double probability) {
    if (values.empty()) throw std::invalid_argument("percentile requires values");
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::clamp(probability, 0.0, 1.0) * static_cast<double>(values.size() - 1U));
    return values[index];
}

PipelineResult run_pipeline(const EnvironmentalContract& input, std::size_t samples = 20000U) {
    validate_contract(input);
    if (samples < 1000U) throw std::invalid_argument("at least 1000 samples are required");

    std::mt19937_64 generator(0x5202601ULL);
    std::normal_distribution<double> standard_normal(0.0, 1.0);

    std::vector<double> water_risks;
    std::vector<double> heat_risks;
    std::vector<double> biodiversity_risks;
    std::vector<double> impacts;
    water_risks.reserve(samples);
    heat_risks.reserve(samples);
    biodiversity_risks.reserve(samples);
    impacts.reserve(samples);

    const auto draw = [&](double mean, double deviation) {
        return mean + deviation * standard_normal(generator);
    };

    for (std::size_t i = 0; i < samples; ++i) {
        const double storage =
            draw(input.storage_m3, input.storage_standard_deviation_m3) +
            draw(input.precipitation_m3, input.precipitation_standard_deviation_m3) +
            draw(input.inflow_m3, input.inflow_standard_deviation_m3) -
            draw(input.outflow_m3, input.outflow_standard_deviation_m3) -
            draw(input.evapotranspiration_m3, input.evapotranspiration_standard_deviation_m3) -
            draw(input.seepage_m3, input.seepage_standard_deviation_m3);

        const double soil_temperature = draw(
            input.soil_temperature_c, input.soil_temperature_standard_deviation_c);

        const double water_risk = storage < 0.0 ? 1.0 : clamp01(1.0 - storage / 100.0);
        const double heat_risk = clamp01((soil_temperature - 30.0) / 15.0);
        const double biodiversity_risk = clamp01(
            0.45 * water_risk +
            0.25 * heat_risk +
            0.20 * (1.0 - input.habitat_connectivity) +
            0.10 * input.fragmentation);

        const double impact = clamp01(
            input.telemetry_quality * (1.0 - biodiversity_risk));

        water_risks.push_back(water_risk);
        heat_risks.push_back(heat_risk);
        biodiversity_risks.push_back(biodiversity_risk);
        impacts.push_back(impact);
    }

    const auto mean = [](const std::vector<double>& values) {
        double total = 0.0;
        for (double value : values) total += value;
        return total / static_cast<double>(values.size());
    };

    const double knowledge = clamp01(
        input.telemetry_quality *
        (1.0 - std::max({
            percentile(water_risks, 0.95),
            percentile(heat_risks, 0.95),
            percentile(biodiversity_risks, 0.95)
        })));

    const double impact_p05 = percentile(impacts, 0.05);
    const std::string action =
        knowledge < 0.40 || impact_p05 < 0.35 ? "HALT" :
        knowledge < 0.70 || impact_p05 < 0.55 ? "DERATE" : "PROCEED";

    return {
        mean(water_risks), percentile(water_risks, 0.95),
        mean(heat_risks), percentile(heat_risks, 0.95),
        mean(biodiversity_risks), percentile(biodiversity_risks, 0.95),
        knowledge, mean(impacts), impact_p05, action
    };
}

}  // namespace eco_restoration

int main() {
    using namespace eco_restoration;

    const EnvironmentalContract input{
        1'770'000'000,
        65.0, 4.0,
        8.0, 1.5,
        18.0, 2.0,
        20.0, 2.0,
        7.0, 1.0,
        4.0, 0.8,
        33.0, 1.8,
        0.72, 0.18, 0.94
    };

    try {
        const PipelineResult result = run_pipeline(input);
        std::cout << std::fixed << std::setprecision(6)
                  << "{\"water_risk_mean\":" << result.water_risk_mean
                  << ",\"water_risk_p95\":" << result.water_risk_p95
                  << ",\"heat_risk_mean\":" << result.heat_risk_mean
                  << ",\"heat_risk_p95\":" << result.heat_risk_p95
                  << ",\"biodiversity_risk_mean\":" << result.biodiversity_risk_mean
                  << ",\"biodiversity_risk_p95\":" << result.biodiversity_risk_p95
                  << ",\"knowledge_factor\":" << result.knowledge_factor
                  << ",\"eco_impact_mean\":" << result.eco_impact_mean
                  << ",\"eco_impact_p05\":" << result.eco_impact_p05
                  << ",\"action\":\"" << result.action << "\"}\n";
    } catch (const std::exception& error) {
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
