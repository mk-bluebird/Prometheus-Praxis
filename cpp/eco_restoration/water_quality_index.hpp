// File: cpp/eco_restoration/water_quality_index.hpp
#pragma once

namespace prometheus { namespace eco {

struct WaterSample {
    double dissolved_oxygen_mg_L;
    double biochemical_oxygen_demand_mg_L;
    double nitrate_mg_L;
    double phosphate_mg_L;
    double turbidity_NTU;
};

class WaterQualityIndex {
public:
    double compute_index(const WaterSample &s) const;
};

} } // namespace prometheus::eco
