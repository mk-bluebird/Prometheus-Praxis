// File: cpp/eco_restoration/quality_weighted_lane_evaluator.cpp
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace eco_restoration {

enum class TelemetryQuality { Good, Suspect, Bad };
enum class LaneDecision { Proceed, Derate, Halt };

struct QualityWeightedLaneInput {
    double base_knowledge_factor{};
    double restoration_value{};
    double heat_risk{};
    double water_risk{};
    double energy_risk{};
    TelemetryQuality heat_quality{};
    TelemetryQuality water_quality{};
    TelemetryQuality energy_quality{};
};

struct QualityWeightedLaneResult {
    double knowledge_factor{};
    double eco_impact_value{};
    double combined_risk{};
    LaneDecision decision{};
};

double quality_weight(TelemetryQuality quality) {
    switch (quality) {
        case TelemetryQuality::Good: return 1.0;
        case TelemetryQuality::Suspect: return 0.60;
        case TelemetryQuality::Bad: return 0.20;
    }
    return 0.20;
}

QualityWeightedLaneResult evaluate_quality_weighted_lane(const QualityWeightedLaneInput& in) {
    if (in.base_knowledge_factor < 0.0 || in.base_knowledge_factor > 1.0 ||
        in.restoration_value < 0.0 || in.restoration_value > 1.0)
        throw std::invalid_argument("invalid lane K/E inputs");

    const auto conservative_risk = [](double risk, double weight) {
        return std::clamp(weight * std::clamp(risk, 0.0, 1.0) + (1.0 - weight), 0.0, 1.0);
    };

    const double q_heat = quality_weight(in.heat_quality);
    const double q_water = quality_weight(in.water_quality);
    const double q_energy = quality_weight(in.energy_quality);
    const double knowledge = std::clamp(in.base_knowledge_factor *
        std::cbrt(q_heat * q_water * q_energy), 0.0, 1.0);
    const double risk = std::max({conservative_risk(in.heat_risk, q_heat),
                                  conservative_risk(in.water_risk, q_water),
                                  conservative_risk(in.energy_risk, q_energy)});
    const double impact = knowledge * in.restoration_value * (1.0 - risk);
    const LaneDecision decision = risk > 0.70 || knowledge < 0.35 ? LaneDecision::Halt :
                                  risk > 0.35 || impact < 0.55 ? LaneDecision::Derate :
                                                                    LaneDecision::Proceed;
    return {knowledge, impact, risk, decision};
}

}  // namespace eco_restoration
