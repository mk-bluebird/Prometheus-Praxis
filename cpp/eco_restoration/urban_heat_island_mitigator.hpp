// File: cpp/eco_restoration/urban_heat_island_mitigator.hpp
#pragma once

#include <string>
#include <vector>

namespace prometheus { namespace eco {

struct UrbanBlock {
    std::string id;
    double impervious_fraction;
    double tree_canopy_fraction;
    double albedo;
};

class UrbanHeatIslandMitigator {
public:
    double estimate_temperature_reduction(const UrbanBlock &block, double baseline_temp_C) const;
};

} } // namespace prometheus::eco
