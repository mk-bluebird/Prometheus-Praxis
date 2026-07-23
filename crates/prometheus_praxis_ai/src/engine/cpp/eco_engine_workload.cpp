// filename: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_workload.cpp

#include "eco_engine_workload.hpp"

namespace {

inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

} // namespace

extern "C" int compute_workload_residual(
    const double* input_data,
    size_t data_size,
    CWorkloadRiskCoords* out_risks,
    CWorkloadResidual* out_residual
) {
    if (input_data == nullptr || out_risks == nullptr || out_residual == nullptr) {
        return 1;
    }
    if (data_size < 4) {
        return 2;
    }

    double energyreqJ = input_data[0];
    double energysurplusJ = input_data[1];
    double hydraulicrisk = input_data[2];
    double uncertaintyrisk = input_data[3];

    if (energyreqJ < 0.0 || energyreqJ > 1.0e9) return 3;
    if (energysurplusJ < 0.0) return 4;
    if (hydraulicrisk < 0.0 || hydraulicrisk > 1.0) return 5;
    if (uncertaintyrisk < 0.0 || uncertaintyrisk > 1.0) return 6;

    double rt = 0.0;
    if (energyreqJ > 0.0) {
        rt = energysurplusJ / energyreqJ;
        if (rt > 2.5) rt = 2.5;
    }

    double r_energy;
    if (energyreqJ <= 0.0) {
        r_energy = 1.0;
    } else if (rt >= 1.2) {
        r_energy = 0.2;
    } else if (rt <= 0.0) {
        r_energy = 0.9;
    } else {
        r_energy = 0.5;
    }

    double r_hydraulics = hydraulicrisk;
    double r_uncertainty = uncertaintyrisk;

    r_energy = clamp01(r_energy);
    r_hydraulics = clamp01(r_hydraulics);
    r_uncertainty = clamp01(r_uncertainty);

    out_risks->r_energy = r_energy;
    out_risks->r_hydraulics = r_hydraulics;
    out_risks->r_uncertainty = r_uncertainty;

    const double w_energy = 0.8;
    const double w_hydraulics = 1.0;
    const double w_uncertainty = 0.6;

    double vt_before = 0.0;
    double vt_after =
        w_energy * r_energy * r_energy +
        w_hydraulics * r_hydraulics * r_hydraulics +
        w_uncertainty * r_uncertainty * r_uncertainty;

    if (vt_after < 0.0) vt_after = 0.0;

    double delta_vt = vt_after - vt_before;

    out_residual->energyreqJ = energyreqJ;
    out_residual->delta_vt = delta_vt;

    return 0;
}
