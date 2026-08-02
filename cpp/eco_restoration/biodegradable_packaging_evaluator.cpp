// File: cpp/eco_restoration/biodegradable_packaging_evaluator.cpp
#include <iostream>
#include <string>
#include <cmath>
#include "biodegradable_packaging_evaluator.hpp"

namespace prometheus { namespace eco {

PackagingScore BiodegradablePackagingEvaluator::evaluate(const PackagingDesign &p) const {
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

} } // namespace prometheus::eco
