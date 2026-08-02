// File: cpp/eco_restoration/decomposition_rate_model.cpp
#include <iostream>
#include <cmath>
#include <string>
#include "decomposition_rate_model.hpp"

namespace prometheus { namespace eco {

double DecompositionRateModel::estimate_daily_mass_loss(const BiomassProperties &b, const DecompositionContext &ctx) const {
    double temp_factor = std::exp(0.07 * (ctx.temperature_C - 20.0));
    double moisture_factor = std::exp(-std::pow((ctx.moisture_pct - 60.0) / 25.0, 2));
    double cn_factor = 1.0 / (1.0 + std::pow((ctx.c_to_n_ratio - 25.0) / 20.0, 2));
    double size_factor = 1.0 / (1.0 + ctx.particle_size_mm / 20.0);
    double lignin_factor = 1.0 - b.lignin_fraction;

    double base_rate = 0.01; // 1% per day baseline
    double rate = base_rate * temp_factor * moisture_factor * cn_factor * size_factor * lignin_factor;
    rate = std::clamp(rate, 0.0, 0.1); // max 10% per day
    return b.initial_mass_kg * rate;
}

} } // namespace prometheus::eco
