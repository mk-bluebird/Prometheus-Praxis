// File: cpp/eco_restoration/urban_heat_island_mitigator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

namespace eco {

struct UrbanBlock {
    std::string id;
    double impervious_fraction;
    double tree_canopy_fraction;
    double albedo;
};

class UrbanHeatIslandMitigator {
public:
    double estimate_temperature_reduction(const UrbanBlock &block,
                                          double baseline_temp_C) const {
        double canopy_effect = 3.0 * block.tree_canopy_fraction;
        double albedo_effect = 2.0 * (block.albedo - 0.15);
        double impervious_penalty = 4.0 * block.impervious_fraction;
        double reduction = canopy_effect + albedo_effect - impervious_penalty;
        if (reduction < 0.0) reduction = 0.0;
        return reduction;
    }
};

} // namespace eco

int main() {
    eco::UrbanHeatIslandMitigator mitigator;
    eco::UrbanBlock block{"Block-1", 0.7, 0.25, 0.3};
    double baseline = 42.0;
    double reduction = mitigator.estimate_temperature_reduction(block, baseline);
    std::cout << "Estimated temperature reduction: " << reduction << " C\n";
    return 0;
}
