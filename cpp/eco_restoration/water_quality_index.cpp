// File: cpp/eco_restoration/water_quality_index.cpp
#include <iostream>
#include <cmath>

namespace eco {

struct WaterSample {
    double dissolved_oxygen_mg_L;
    double biochemical_oxygen_demand_mg_L;
    double nitrate_mg_L;
    double phosphate_mg_L;
    double turbidity_NTU;
};

class WaterQualityIndex {
public:
    double compute_index(const WaterSample &s) const {
        double do_term = s.dissolved_oxygen_mg_L / 8.0;
        do_term = std::clamp(do_term, 0.0, 1.0);
        double bod_term = 1.0 - s.biochemical_oxygen_demand_mg_L / 15.0;
        bod_term = std::clamp(bod_term, 0.0, 1.0);
        double nutrient_term = 1.0 / (1.0 + (s.nitrate_mg_L + s.phosphate_mg_L) / 5.0);
        double turbidity_term = 1.0 / (1.0 + s.turbidity_NTU / 20.0);
        return 0.3 * do_term + 0.25 * bod_term + 0.25 * nutrient_term + 0.2 * turbidity_term;
    }
};

} // namespace eco

int main() {
    eco::WaterQualityIndex idx;
    eco::WaterSample sample{7.5, 3.0, 1.2, 0.3, 4.0};
    std::cout << "Water quality index: " << idx.compute_index(sample) << "\n";
    return 0;
}
