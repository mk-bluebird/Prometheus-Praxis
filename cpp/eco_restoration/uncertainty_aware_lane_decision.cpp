// File: cpp/eco_restoration/uncertainty_aware_lane_decision.cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

enum class LaneAction { Proceed, Derate, Halt };

struct EnvironmentalContract {
    double knowledge_factor{};
    double restoration_value{};
    double heat_risk{};
    double water_risk{};
    double biodiversity_risk{};
    double energy_risk{};
    double risk_standard_deviation{};
    double knowledge_standard_deviation{};
    double verified_benefit_g{};
    double attributable_carbon_g{};
    double lyapunov_delta{};
    double confidence{};
};

struct UncertaintyLaneDecision {
    LaneAction action{};
    double action_confidence{};
    double risk_p05{};
    double risk_p95{};
    double knowledge_p05{};
    double knowledge_p95{};
    double eco_impact_mean{};
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

LaneAction evaluate_action(double knowledge, double impact, double risk,
                           double benefit_g, double carbon_g, double lyapunov_delta) {
    if (benefit_g <= carbon_g || lyapunov_delta > 0.02 || risk > 0.70 || knowledge < 0.35)
        return LaneAction::Halt;
    if (risk > 0.35 || impact < 0.55) return LaneAction::Derate;
    return LaneAction::Proceed;
}

double quantile(std::vector<double> values, double probability) {
    const std::size_t index = static_cast<std::size_t>(
        std::clamp(probability, 0.0, 1.0) * static_cast<double>(values.size() - 1));
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

UncertaintyLaneDecision evaluate_uncertainty_aware_lane(
    const EnvironmentalContract& contract, std::size_t samples = 10000,
    std::uint64_t seed = 20260812ULL) {
    if (samples < 1000 || contract.risk_standard_deviation < 0.0 ||
        contract.knowledge_standard_deviation < 0.0 || contract.confidence < 0.0 ||
        contract.confidence > 1.0 || contract.verified_benefit_g < 0.0 ||
        contract.attributable_carbon_g < 0.0 || contract.lyapunov_delta < 0.0)
        throw std::invalid_argument("invalid uncertainty contract");

    std::mt19937_64 generator(seed);
    std::normal_distribution<double> risk_noise(0.0, contract.risk_standard_deviation);
    std::normal_distribution<double> knowledge_noise(0.0, contract.knowledge_standard_deviation);
    std::vector<double> risks, knowledge, impacts;
    risks.reserve(samples); knowledge.reserve(samples); impacts.reserve(samples);
    std::array<std::size_t, 3> actions{};

    for (std::size_t i = 0; i < samples; ++i) {
        const double k = clamp01(contract.knowledge_factor * contract.confidence + knowledge_noise(generator));
        const double risk = clamp01(std::max({contract.heat_risk, contract.water_risk,
                                              contract.biodiversity_risk, contract.energy_risk}) +
                                    risk_noise(generator));
        const double impact = k * clamp01(contract.restoration_value) * (1.0 - risk);
        ++actions[static_cast<std::size_t>(evaluate_action(
            k, impact, risk, contract.verified_benefit_g, contract.attributable_carbon_g,
            contract.lyapunov_delta))];
        risks.push_back(risk); knowledge.push_back(k); impacts.push_back(impact);
    }

    const std::size_t dominant = std::distance(actions.begin(),
        std::max_element(actions.begin(), actions.end()));
    const std::size_t selected = actions[static_cast<std::size_t>(LaneAction::Halt)] > samples / 20
        ? static_cast<std::size_t>(LaneAction::Halt) : dominant;
    double impact_mean = 0.0;
    for (double impact : impacts) impact_mean += impact;
    return {static_cast<LaneAction>(selected), static_cast<double>(actions[selected]) / samples,
            quantile(risks, 0.05), quantile(risks, 0.95), quantile(knowledge, 0.05),
            quantile(knowledge, 0.95), impact_mean / samples};
}

}  // namespace eco_restoration
