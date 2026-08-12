// File: cpp/eco_restoration/research_corridor_registry.cpp

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

enum class CorridorStatus {
    Proceed,
    Investigate,
    Halt
};

struct ResearchObservation {
    double soil_organic_carbon_delta_kg_m2{};
    double infiltration_mm_h{};
    double runoff_mm_h{};
    double water_use_l{};
    double water_replenished_l{};
    double native_habitat_connectivity{};
    double native_species_survival{};
    double contaminant_mg_l{};
    double material_biodegradation_fraction{};
    double worker_heat_c{};
    double community_benefit_score{};
    double measurement_uncertainty{};
    double ecological_evidence_score{};
};

struct CorridorDefinition {
    std::string id;
    std::string purpose;
    double minimum_value{};
    double maximum_value{};
    double knowledge_weight{};
    double eco_weight{};
};

struct CorridorResult {
    std::string id;
    double value{};
    double normalized_safety{};
    CorridorStatus status{};
};

struct ResearchAssessment {
    std::vector<CorridorResult> corridors;
    double knowledge_factor{};
    double eco_impact_value{};
    CorridorStatus overall_status{};
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double normalized_minimum(double value, double target) {
    if (target <= 0.0) {
        throw std::invalid_argument("minimum target must be positive");
    }
    return clamp01(value / target);
}

double normalized_maximum(double value, double limit) {
    if (limit <= 0.0) {
        throw std::invalid_argument("maximum limit must be positive");
    }
    return clamp01(1.0 - value / limit);
}

CorridorStatus classify(double safety) {
    if (safety >= 0.80) {
        return CorridorStatus::Proceed;
    }
    if (safety >= 0.50) {
        return CorridorStatus::Investigate;
    }
    return CorridorStatus::Halt;
}

const char* name_of(CorridorStatus status) {
    switch (status) {
        case CorridorStatus::Proceed: return "PROCEED";
        case CorridorStatus::Investigate: return "INVESTIGATE";
        case CorridorStatus::Halt: return "HALT";
    }
    return "HALT";
}

CorridorResult minimum_corridor(
    const CorridorDefinition& definition,
    double observed_value) {

    const double safety = normalized_minimum(observed_value, definition.minimum_value);
    return {definition.id, observed_value, safety, classify(safety)};
}

CorridorResult maximum_corridor(
    const CorridorDefinition& definition,
    double observed_value) {

    const double safety = normalized_maximum(observed_value, definition.maximum_value);
    return {definition.id, observed_value, safety, classify(safety)};
}

ResearchAssessment assess(const ResearchObservation& observation) {
    if (observation.soil_organic_carbon_delta_kg_m2 < 0.0 ||
        observation.infiltration_mm_h < 0.0 ||
        observation.runoff_mm_h < 0.0 ||
        observation.water_use_l < 0.0 ||
        observation.water_replenished_l < 0.0 ||
        observation.contaminant_mg_l < 0.0 ||
        observation.worker_heat_c < 0.0 ||
        observation.measurement_uncertainty < 0.0) {
        throw std::invalid_argument("physical measurements must be nonnegative");
    }

    const CorridorDefinition carbon{
        "soil_carbon", "Retain measurable soil organic carbon", 0.02, 0.0, 1.2, 1.4
    };
    const CorridorDefinition infiltration{
        "infiltration", "Increase water entry into restoration soils", 4.0, 0.0, 1.0, 1.1
    };
    const CorridorDefinition runoff{
        "runoff", "Limit erosive surface runoff", 0.0, 2.0, 0.9, 1.2
    };
    const CorridorDefinition water{
        "water_balance", "Avoid net withdrawal from the restoration site", 1.0, 0.0, 1.1, 1.3
    };
    const CorridorDefinition habitat{
        "habitat_connectivity", "Maintain connected native habitat", 0.65, 0.0, 1.3, 1.4
    };
    const CorridorDefinition survival{
        "native_survival", "Confirm native-species establishment", 0.75, 0.0, 1.3, 1.4
    };
    const CorridorDefinition contaminant{
        "contaminant", "Keep monitored contaminants below the site limit", 0.0, 0.05, 1.4, 1.5
    };
    const CorridorDefinition material{
        "material_recovery", "Prefer biodegradable and recoverable material flows", 0.80, 0.0, 0.9, 1.0
    };
    const CorridorDefinition heat{
        "worker_heat", "Keep worker environmental heat within the operating limit", 0.0, 39.0, 1.0, 1.1
    };
    const CorridorDefinition community{
        "community_benefit", "Demonstrate community-accessible restoration value", 0.60, 0.0, 0.8, 1.0
    };

    const double water_ratio = observation.water_replenished_l /
        std::max(observation.water_use_l, 1.0);

    std::vector<CorridorResult> results;
    results.push_back(minimum_corridor(carbon, observation.soil_organic_carbon_delta_kg_m2));
    results.push_back(minimum_corridor(infiltration, observation.infiltration_mm_h));
    results.push_back(maximum_corridor(runoff, observation.runoff_mm_h));
    results.push_back(minimum_corridor(water, water_ratio));
    results.push_back(minimum_corridor(habitat, clamp01(observation.native_habitat_connectivity)));
    results.push_back(minimum_corridor(survival, clamp01(observation.native_species_survival)));
    results.push_back(maximum_corridor(contaminant, observation.contaminant_mg_l));
    results.push_back(minimum_corridor(material, clamp01(observation.material_biodegradation_fraction)));
    results.push_back(maximum_corridor(heat, observation.worker_heat_c));
    results.push_back(minimum_corridor(community, clamp01(observation.community_benefit_score)));

    double knowledge_sum = 0.0;
    double knowledge_weight_sum = 0.0;
    double eco_sum = 0.0;
    double eco_weight_sum = 0.0;
    const std::vector<CorridorDefinition> definitions{
        carbon, infiltration, runoff, water, habitat, survival, contaminant, material, heat, community
    };

    for (std::size_t i = 0; i < results.size(); ++i) {
        knowledge_sum += results[i].normalized_safety * definitions[i].knowledge_weight;
        knowledge_weight_sum += definitions[i].knowledge_weight;
        eco_sum += results[i].normalized_safety * definitions[i].eco_weight;
        eco_weight_sum += definitions[i].eco_weight;
    }

    const double uncertainty_penalty = 1.0 - clamp01(observation.measurement_uncertainty);
    const double evidence = clamp01(observation.ecological_evidence_score);
    const double knowledge = clamp01((knowledge_sum / knowledge_weight_sum) * uncertainty_penalty);
    const double eco_value = clamp01((eco_sum / eco_weight_sum) * knowledge * evidence);

    CorridorStatus overall = CorridorStatus::Proceed;
    for (const CorridorResult& result : results) {
        if (result.status == CorridorStatus::Halt) {
            overall = CorridorStatus::Halt;
            break;
        }
        if (result.status == CorridorStatus::Investigate) {
            overall = CorridorStatus::Investigate;
        }
    }

    return {std::move(results), knowledge, eco_value, overall};
}

}  // namespace eco_restoration

int main() {
    using namespace eco_restoration;

    const ResearchObservation observation{
        0.035, 6.2, 0.8, 240.0, 290.0, 0.74, 0.81,
        0.012, 0.89, 34.0, 0.78, 0.08, 0.86
    };

    try {
        const ResearchAssessment assessment = assess(observation);
        std::cout << std::fixed << std::setprecision(4)
                  << "{\"knowledge_factor\":" << assessment.knowledge_factor
                  << ",\"eco_impact_value\":" << assessment.eco_impact_value
                  << ",\"status\":\"" << name_of(assessment.overall_status)
                  << "\",\"corridors\":[";

        for (std::size_t i = 0; i < assessment.corridors.size(); ++i) {
            const CorridorResult& result = assessment.corridors[i];
            std::cout << "{\"id\":\"" << result.id
                      << "\",\"value\":" << result.value
                      << ",\"safety\":" << result.normalized_safety
                      << ",\"status\":\"" << name_of(result.status) << "\"}";
            if (i + 1U < assessment.corridors.size()) {
                std::cout << ',';
            }
        }
        std::cout << "]}\n";
    } catch (const std::exception& error) {
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
