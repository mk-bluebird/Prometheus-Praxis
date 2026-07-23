// filename: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_ai_node.cpp

#include "eco_engine_ai_node.hpp"

namespace {

inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

} // namespace

extern "C" int compute_ai_node_residual(
    double pue,
    double cue,
    double power_kw,
    double cooling_kw,
    double thermal_output_kw,
    double carbon_intensity,
    double biodiversity_risk,
    double uncertainty_risk,
    CAiNodeRiskCoords* out_risks,
    CAiNodeResidualSlice* out_residual
) {
    if (out_risks == nullptr || out_residual == nullptr) {
        return 1;
    }

    if (pue < 1.0 || pue > 3.5) return 2;
    if (cue < 0.5 || cue > 5.0) return 3;
    if (power_kw < 0.0 || power_kw > 100000.0) return 4;
    if (cooling_kw < 0.0 || cooling_kw > 100000.0) return 5;
    if (thermal_output_kw < 0.0) return 6;
    if (carbon_intensity < 0.0 || carbon_intensity > 1.0) return 7;
    if (biodiversity_risk < 0.0 || biodiversity_risk > 1.0) return 8;
    if (uncertainty_risk < 0.0 || uncertainty_risk > 1.0) return 9;

    double pue_norm = (pue - 1.0) / (3.5 - 1.0);
    double cue_norm = (cue - 0.5) / (5.0 - 0.5);
    double power_norm = power_kw / 100000.0;
    double cooling_norm = cooling_kw / 100000.0;

    double r_energy_compute = clamp01((pue_norm + power_norm) * 0.5);
    double r_cooling_water = clamp01((cue_norm + cooling_norm) * 0.5);
    double r_carbon = clamp01(carbon_intensity);
    double r_biodiversity = clamp01(biodiversity_risk);
    double r_uncertainty = clamp01(uncertainty_risk);

    out_risks->r_energy_compute = r_energy_compute;
    out_risks->r_cooling_water = r_cooling_water;
    out_risks->r_carbon = r_carbon;
    out_risks->r_biodiversity = r_biodiversity;
    out_risks->r_uncertainty = r_uncertainty;

    const double w_energy_compute = 0.7;
    const double w_cooling_water = 0.6;
    const double w_carbon = 1.0;
    const double w_biodiversity = 1.0;
    const double w_uncertainty = 0.8;

    double vt_before_ai = 0.0;
    double vt_after_ai =
        w_energy_compute * r_energy_compute * r_energy_compute +
        w_cooling_water * r_cooling_water * r_cooling_water +
        w_carbon * r_carbon * r_carbon +
        w_biodiversity * r_biodiversity * r_biodiversity +
        w_uncertainty * r_uncertainty * r_uncertainty;

    if (vt_after_ai < 0.0) vt_after_ai = 0.0;

    double delta_vt_ai = vt_after_ai - vt_before_ai;

    out_residual->vt_before_ai = vt_before_ai;
    out_residual->vt_after_ai = vt_after_ai;
    out_residual->delta_vt_ai = delta_vt_ai;

    return 0;
}
