// File: cpp/eco_restoration/material_eco_score_model.hpp
#pragma once

#include <string>

namespace prometheus { namespace eco {

struct MaterialProperties {
    std::string name;
    double density_kg_m3;
    double embodied_energy_MJ_kg;
    double biodegradation_half_life_days;
    double toxicity_index;          // 0.0 (non-toxic) .. 1.0 (highly toxic)
    double recyclable_fraction;     // 0.0 .. 1.0
};

struct EcoScore {
    double knowledge_factor;        // 0.0 .. 1.0 based on data completeness
    double eco_impact_value;        // higher is better (eco-positive)
};

class MaterialEcoScoreModel {
public:
    EcoScore compute_score(const MaterialProperties &props) const;
};

} } // namespace prometheus::eco
