// File: cpp/eco_restoration/leaching_risk_index_calibration.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// Leaching Risk Index (LRI):
//   LRI = (precip_intensity × soil_sand_fraction) / (organic_matter_pct × (1 + microbial_activity))
//
// Variables:
//   - precip_intensity       : mm of rainfall per event or per hour (desert wash pulses).
//   - soil_sand_fraction     : fraction of sand in soil texture [0,1].
//   - organic_matter_pct     : percentage of organic matter in soil (0–100).
//   - microbial_activity     : normalized index [0,1] derived from respiration or enzyme assays.
//
// This file computes LRI and sketches a calibration protocol using pore‑water samplers.

struct LeachingContext {
    double precip_intensity_mm;
    double soil_sand_fraction;
    double organic_matter_pct;
    double microbial_activity_index;
};

double compute_lri(const LeachingContext& c) {
    double denom = c.organic_matter_pct * (1.0 + c.microbial_activity_index);
    if (denom <= 0.0) {
        return 0.0;
    }
    return (c.precip_intensity_mm * c.soil_sand_fraction) / denom;
}

// Calibration data from pore‑water samplers (field measurements).
struct PoreWaterSample {
    double lri_model;
    double leachate_concentration_mg_per_L; // measured contaminant or nutrient concentration
};

// Fit a simple linear calibration factor: leachate ≈ k * LRI.
double calibrate_lri_factor(const std::vector<PoreWaterSample>& samples) {
    double sum_lri = 0.0;
    double sum_leachate = 0.0;
    double sum_lri2 = 0.0;
    double sum_lri_leach = 0.0;
    int n = 0;

    for (const auto& s : samples) {
        if (s.lri_model <= 0.0) continue;
        sum_lri += s.lri_model;
        sum_leachate += s.leachate_concentration_mg_per_L;
        sum_lri2 += s.lri_model * s.lri_model;
        sum_lri_leach += s.lri_model * s.leachate_concentration_mg_per_L;
        ++n;
    }

    if (n == 0) {
        return 1.0;
    }

    double mean_lri = sum_lri / n;
    double mean_leach = sum_leachate / n;
    double cov = (sum_lri_leach / n) - mean_lri * mean_leach;
    double var = (sum_lri2 / n) - mean_lri * mean_lri;

    if (var <= 0.0) {
        return 1.0;
    }

    double k = cov / var;
    if (k <= 0.0) {
        k = 1.0;
    }
    return k;
}

// Example calibration protocol sketch:
// 1. Install pore‑water samplers at multiple depths and distances along a Phoenix desert wash.
// 2. During monsoon events, record precip_intensity_mm and retrieve pore‑water samples.
// 3. Measure contaminant/nutrient concentrations (mg/L).
// 4. For each location/time, compute LRI from soil texture, organic matter, and microbial activity.
// 5. Fit calibration factor k using calibrate_lri_factor().
// 6. Use calibrated LRI to set corridor bands and KER risk coordinates for leachate.

int main() {
    LeachingContext ctx;
    ctx.precip_intensity_mm = 25.0;
    ctx.soil_sand_fraction = 0.75;
    ctx.organic_matter_pct = 2.5;
    ctx.microbial_activity_index = 0.3;

    double lri = compute_lri(ctx);
    std::cout << "Raw LRI = " << lri << "\n";

    std::vector<PoreWaterSample> samples;
    samples.push_back(PoreWaterSample{lri, 1.8});
    samples.push_back(PoreWaterSample{lri * 1.1, 2.0});
    samples.push_back(PoreWaterSample{lri * 0.9, 1.6});

    double k = calibrate_lri_factor(samples);
    std::cout << "Calibrated LRI factor k = " << k << "\n";

    double predicted_leachate = k * lri;
    std::cout << "Predicted leachate concentration ≈ " << predicted_leachate << " mg/L\n";

    return 0;
}
