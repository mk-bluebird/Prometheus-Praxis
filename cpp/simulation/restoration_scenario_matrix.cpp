// File: cpp/simulation/restoration_scenario_matrix.cpp
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct ClimateScenario {
    std::string id;
    double precipitation_mm_day{};
    double reference_et_mm_day{};
    double heat_stress{};
};

struct RestorationAction {
    std::string id;
    double infiltration_fraction{};
    double mulch_et_reduction{};
    double habitat_connectivity_gain{};
    double native_survival_gain{};
};

struct ScenarioOutcome {
    std::string climate_id;
    std::string action_id;
    double water_balance_mm_day{};
    double biodiversity_score{};
    double risk{};
    double knowledge_factor{};
    double eco_impact_value{};
};

std::vector<ScenarioOutcome> run_restoration_scenario_matrix(
    const std::vector<ClimateScenario>& climates, const std::vector<RestorationAction>& actions,
    double base_infiltration_fraction, double base_connectivity, double base_survival,
    double evidence_confidence) {
    if (climates.empty() || actions.empty() || evidence_confidence < 0.0 || evidence_confidence > 1.0)
        throw std::invalid_argument("invalid scenario matrix inputs");

    std::vector<ScenarioOutcome> results;
    for (const auto& climate : climates) for (const auto& action : actions) {
        const double infiltration = std::clamp(base_infiltration_fraction + action.infiltration_fraction, 0.0, 1.0);
        const double actual_et = std::max(0.0, climate.reference_et_mm_day * (1.0 - action.mulch_et_reduction));
        const double water_balance = climate.precipitation_mm_day * infiltration - actual_et;
        const double connectivity = std::clamp(base_connectivity + action.habitat_connectivity_gain, 0.0, 1.0);
        const double survival = std::clamp(base_survival + action.native_survival_gain +
                                           0.08 * std::tanh(water_balance), 0.0, 1.0);
        const double biodiversity = 0.5 * connectivity + 0.5 * survival;
        const double risk = std::clamp(0.55 * std::max(0.0, -water_balance) /
                                       std::max(1.0, climate.reference_et_mm_day) +
                                       0.45 * climate.heat_stress, 0.0, 1.0);
        const double knowledge = evidence_confidence;
        results.push_back({climate.id, action.id, water_balance, biodiversity, risk,
                           knowledge, knowledge * biodiversity * (1.0 - risk)});
    }
    return results;
}

}  // namespace eco_restoration
