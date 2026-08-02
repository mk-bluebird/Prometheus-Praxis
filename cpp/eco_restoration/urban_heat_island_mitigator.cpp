// File: cpp/eco_restoration/urban_heat_island_mitigator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "urban_heat_island_mitigator.hpp"

namespace prometheus { namespace eco {

double UrbanHeatIslandMitigator::estimate_temperature_reduction(const UrbanBlock &block, double baseline_temp_C) const {
    double canopy_effect = 3.0 * block.tree_canopy_fraction;
    double albedo_effect = 2.0 * (block.albedo - 0.15);
    double impervious_penalty = 4.0 * block.impervious_fraction;
    double reduction = canopy_effect + albedo_effect - impervious_penalty;
    if (reduction < 0.0) reduction = 0.0;
    return reduction;
}

} } // namespace prometheus::eco
