// File: cpp/eco_restoration/material_eco_score_model.cpp
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

namespace eco {

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
    EcoScore compute_score(const MaterialProperties &props) const {
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
};

} // namespace eco

int main() {
    eco::MaterialEcoScoreModel model;
    eco::MaterialProperties bamboo {
        "Bamboo", 700.0, 5.0, 90.0, 0.1, 0.8
    };
    eco::EcoScore score = model.compute_score(bamboo);
    std::cout << "Material: " << bamboo.name << "\n";
    std::cout << "Knowledge factor: " << score.knowledge_factor << "\n";
    std::cout << "Eco impact value: " << score.eco_impact_value << "\n";
    return 0;
}
