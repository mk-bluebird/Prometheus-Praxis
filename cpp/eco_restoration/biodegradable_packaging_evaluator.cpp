// File: cpp/eco_restoration/biodegradable_packaging_evaluator.cpp
#include <iostream>
#include <string>
#include <cmath>

namespace eco {

struct PackagingDesign {
    std::string name;
    double material_mass_kg;
    double biodegradation_half_life_days;
    double microplastic_risk_index;
    double renewable_content_fraction;
};

struct PackagingScore {
    double knowledge_factor;
    double eco_impact_value;
};

class BiodegradablePackagingEvaluator {
public:
    PackagingScore evaluate(const PackagingDesign &p) const {
        PackagingScore s{};
        s.knowledge_factor = 0.9; // simple fixed factor for now

        double degradation_term = 1.0 / (1.0 + std::log10(p.biodegradation_half_life_days + 1.0));
        double microplastic_term = 1.0 - p.microplastic_risk_index;
        double renewable_term = p.renewable_content_fraction;
        double mass_penalty = 1.0 / (1.0 + p.material_mass_kg);

        s.eco_impact_value = 0.35 * degradation_term
                           + 0.3 * microplastic_term
                           + 0.25 * renewable_term
                           + 0.10 * mass_penalty;
        return s;
    }
};

} // namespace eco

int main() {
    eco::BiodegradablePackagingEvaluator eval;
    eco::PackagingDesign design{"Compostable cup", 0.02, 120.0, 0.1, 0.9};
    auto score = eval.evaluate(design);
    std::cout << "Packaging eco-impact: " << score.eco_impact_value << "\n";
    return 0;
}
