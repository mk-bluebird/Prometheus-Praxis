// File: cpp/eco_restoration/decomposition_rate_model.hpp
#pragma once

#include <string>

namespace prometheus { namespace eco {

struct DecompositionContext {
    double temperature_C;
    double moisture_pct;
    double c_to_n_ratio;
    double particle_size_mm;
};

struct BiomassProperties {
    std::string name;
    double lignin_fraction;
    double initial_mass_kg;
};

class DecompositionRateModel {
public:
    double estimate_daily_mass_loss(const BiomassProperties &b, const DecompositionContext &ctx) const;
};

} } // namespace prometheus::eco
