// File: cpp/examples/material_eco_example.cpp
#include <iostream>
#include "eco_restoration.hpp"

int main() {
    eco_restoration::MaterialTestParams params{};
    params.oxygen_depletion_percent = 65.0;
    params.co2_evolution_percent    = 62.0;
    params.bod_removal_percent      = 60.0;
    params.doc_removal_percent      = 75.0;
    params.days_to_pass_window      = 9.0;
    params.toxicity_score           = 0.2;
    params.pfas_presence            = 0.1;

    eco_restoration::MaterialEcoImpact impact =
        eco_restoration::compute_material_eco_impact_cpp(params);

    std::cout << "Material eco-impact example:\n"
              << "  k_safe_fraction=" << impact.k_safe_fraction << "\n"
              << "  e_eco_benefit_band=" << impact.e_eco_benefit_band << "\n"
              << "  r_risk_max=" << impact.r_risk_max << "\n"
              << "  ker_score=" << impact.ker_score << "\n"
              << "  biodegradability_score=" << impact.biodegradability_score << "\n";
    return 0;
}
