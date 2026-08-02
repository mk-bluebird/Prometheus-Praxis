// File: cpp/eco_restoration/soil_health_index.hpp
#pragma once

namespace prometheus { namespace eco {

struct SoilSample {
    double organic_matter_pct;
    double ph;
    double moisture_pct;
    double bulk_density_g_cm3;
    double microbial_activity_index; // 0..1
};

class SoilHealthIndex {
public:
    double compute_index(const SoilSample &s) const;
};

} } // namespace prometheus::eco
