// File: cpp/eco_restoration/material_eco_score_model.cpp
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include "material_eco_score_model.hpp"

namespace prometheus { namespace eco {

EcoScore MaterialEcoScoreModel::compute_score(const MaterialProperties &props) const {
    double completeness = 0.0;
    int fields = 0;
    if (props.density_kg_m3 > 0.0) { completeness += 0.2; ++fields; }
    if (props.embodied_energy_MJ_kg > 0.0) { completeness += 0.2; ++fields; }
    if (props.biodegradation_half_life_days > 0.0) { completeness += 0.2; ++fields; }
    completeness += 0.2; // toxicity assumed known
    completeness += 0.2; // recyclable fraction assumed known

    EcoScore score;
    score.knowledge_factor = completeness;

    double energy_term = std::exp(-props.embodied_energy_MJ_kg / 100.0);
    double biodegradation_term = 1.0 / (1.0 + std::log10(props.biodegradation_half_life_days + 1.0));
    double toxicity_term = 1.0 - props.toxicity_index;
    double recycling_term = props.recyclable_fraction;

    score.eco_impact_value = 0.35 * energy_term
                           + 0.25 * biodegradation_term
                           + 0.25 * toxicity_term
                           + 0.15 * recycling_term;
    return score;
}

} } // namespace prometheus::eco
