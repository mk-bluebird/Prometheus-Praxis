// File: cpp/eco_restoration/soil_health_index.cpp
#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <cmath>

namespace eco {

struct SoilSample {
    double organic_matter_pct;
    double ph;
    double moisture_pct;
    double bulk_density_g_cm3;
    double microbial_activity_index; // 0..1
};

class SoilHealthIndex {
public:
    double compute_index(const SoilSample &s) const {
        double om_term = std::min(s.organic_matter_pct / 8.0, 1.0);
        double ph_term = 1.0 - std::abs(s.ph - 6.8) / 3.0;
        ph_term = std::max(ph_term, 0.0);
        double moisture_term = std::exp(-std::pow((s.moisture_pct - 25.0) / 15.0, 2));
        double density_term = 1.0 - (s.bulk_density_g_cm3 - 1.1) / 0.9;
        density_term = std::clamp(density_term, 0.0, 1.0);
        double micro_term = s.microbial_activity_index;

        return 0.3 * om_term + 0.2 * ph_term + 0.2 * moisture_term
             + 0.15 * density_term + 0.15 * micro_term;
    }
};

} // namespace eco

int main() {
    eco::SoilHealthIndex idx;
    eco::SoilSample sample{5.0, 6.7, 22.0, 1.2, 0.7};
    std::cout << "Soil health index: " << idx.compute_index(sample) << "\n";
    return 0;
}
