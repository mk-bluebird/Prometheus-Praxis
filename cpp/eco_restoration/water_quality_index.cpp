// File: cpp/eco_restoration/water_quality_index.cpp
#include <iostream>
#include <cmath>
#include "water_quality_index.hpp"

namespace prometheus { namespace eco {

double WaterQualityIndex::compute_index(const WaterSample &s) const {
    double do_term = s.dissolved_oxygen_mg_L / 8.0;
    do_term = std::clamp(do_term, 0.0, 1.0);
    double bod_term = 1.0 - s.biochemical_oxygen_demand_mg_L / 15.0;
    bod_term = std::clamp(bod_term, 0.0, 1.0);
    double nutrient_term = 1.0 / (1.0 + (s.nitrate_mg_L + s.phosphate_mg_L) / 5.0);
    double turbidity_term = 1.0 / (1.0 + s.turbidity_NTU / 20.0);
    return 0.3 * do_term + 0.25 * bod_term + 0.25 * nutrient_term + 0.2 * turbidity_term;
}

} } // namespace prometheus::eco
