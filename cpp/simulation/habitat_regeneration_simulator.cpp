// File: cpp/simulation/habitat_regeneration_simulator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

namespace eco {

struct HabitatPatch {
    std::string name;
    double area_ha;
    double initial_biodiversity_index;
    double restoration_effort_index;
};

class HabitatRegenerationSimulator {
public:
    explicit HabitatRegenerationSimulator(std::vector<HabitatPatch> patches)
        : patches_(std::move(patches)) {}

    void step_year() {
        for (auto &p : patches_) {
            double effort = std::clamp(p.restoration_effort_index, 0.0, 1.0);
            double growth_rate = 0.05 + 0.15 * effort;
            double remaining_gap = 1.0 - p.initial_biodiversity_index;
            double delta = growth_rate * remaining_gap;
            p.initial_biodiversity_index += delta;
            if (p.initial_biodiversity_index > 1.0) p.initial_biodiversity_index = 1.0;
        }
    }

    void report(int year) const {
        std::cout << "Year " << year << " habitat status:\n";
        for (const auto &p : patches_) {
            std::cout << "  " << p.name << " | area: " << p.area_ha
                      << " ha | biodiversity: " << std::fixed << std::setprecision(3)
                      << p.initial_biodiversity_index << "\n";
        }
    }

private:
    std::vector<HabitatPatch> patches_;
};

} // namespace eco

int main() {
    std::vector<eco::HabitatPatch> patches{
        {"Riparian corridor", 15.0, 0.4, 0.8},
        {"Urban pollinator garden", 2.0, 0.6, 0.9},
        {"Reforestation plot", 50.0, 0.3, 0.7}
    };
    eco::HabitatRegenerationSimulator sim(patches);
    for (int year = 0; year < 10; ++year) {
        sim.step_year();
        sim.report(year);
    }
    return 0;
}
