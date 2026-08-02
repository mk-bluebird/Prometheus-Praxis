// File: cpp/eco_restoration/biodegradable_packaging_evaluator.hpp
#pragma once

#include <string>

namespace prometheus { namespace eco {

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
    PackagingScore evaluate(const PackagingDesign &p) const;
};

} } // namespace prometheus::eco
