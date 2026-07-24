// ecorestorationshard/cyboquaticprogress/20260723-d-cyboquaticworkload/cpp/CyboWorkloadKernel.cpp
// Non-actuating cyboquatic workload energetics kernel.
// Computes normalized risk coordinates and Lyapunov residual slice for energyreqJ and ΔVt.

#include <cstddef>

extern "C" {

// POD structs for FFI and cross-language binding.

struct CyboWorkloadRiskCoords {
    double r_energy;       // normalized energy workload risk in [0,1]
    double r_hydraulics;   // normalized hydraulic coupling risk in [0,1]
    double r_carbon;       // normalized carbon-intensity risk in [0,1]
    double r_uncertainty;  // normalized telemetry uncertainty in [0,1]
};

struct CyboWorkloadResidual {
    double vt_before;  // Lyapunov residual before workload change
    double vt_after;   // Lyapunov residual after workload change
    double delta_vt;   // vt_after - vt_before
};

// Internal helpers.

static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Normalize raw energy (J) to a risk coordinate using a corridor max.
static double normalize_energy(double energy_req_J, double energy_corridor_max_J) {
    if (energy_corridor_max_J <= 0.0) return 1.0;
    double ratio = energy_req_J / energy_corridor_max_J;
    if (ratio < 0.0) ratio = 0.0;
    return clamp01(ratio);
}

// Normalize hydraulic load (dimensionless) to risk.
static double normalize_hydraulics(double hydraulic_load, double hydraulic_corridor_max) {
    if (hydraulic_corridor_max <= 0.0) return 1.0;
    double ratio = hydraulic_load / hydraulic_corridor_max;
    if (ratio < 0.0) ratio = 0.0;
    return clamp01(ratio);
}

// Normalize carbon intensity (kg CO2 / kWh) to risk.
static double normalize_carbon(double carbon_intensity, double carbon_corridor_max) {
    if (carbon_corridor_max <= 0.0) return 1.0;
    double ratio = carbon_intensity / carbon_corridor_max;
    if (ratio < 0.0) ratio = 0.0;
    return clamp01(ratio);
}

// Normalize telemetry uncertainty (0-1) directly.
static double normalize_uncertainty(double uncertainty_raw) {
    return clamp01(uncertainty_raw);
}

// Compute a simple convex Lyapunov residual using quadratic penalty.
// vt = w_e * r_energy^2 + w_h * r_hydraulics^2 + w_c * r_carbon^2 + w_u * r_uncertainty^2
static double compute_vt(
    const CyboWorkloadRiskCoords &risks,
    double w_e,
    double w_h,
    double w_c,
    double w_u
) {
    double re2 = risks.r_energy * risks.r_energy;
    double rh2 = risks.r_hydraulics * risks.r_hydraulics;
    double rc2 = risks.r_carbon * risks.r_carbon;
    double ru2 = risks.r_uncertainty * risks.r_uncertainty;
    double vt = w_e * re2 + w_h * rh2 + w_c * rc2 + w_u * ru2;
    if (vt < 0.0) vt = 0.0;
    return vt;
}

/**
 * Non-actuating cyboquatic workload kernel.
 *
 * Inputs:
 *  - energy_req_J: required energy for workload segment, J >= 0.
 *  - energy_corridor_max_J: corridor maximum energy, J > 0.
 *  - hydraulic_load: dimensionless hydraulic workload (0..hydraulic_corridor_max).
 *  - hydraulic_corridor_max: corridor max for hydraulic load > 0.
 *  - carbon_intensity: kg CO2 per kWh (0..carbon_corridor_max).
 *  - carbon_corridor_max: corridor max for carbon intensity > 0.
 *  - uncertainty_raw: telemetry uncertainty in [0,1].
 *  - vt_before_hint: optional hint for vt_before (e.g., from previous window), if <0 uses computed.
 *
 * Outputs:
 *  - out_risks: normalized risk coordinates.
 *  - out_residual: Lyapunov residual slice; delta_vt must not be positive for non-regressive workloads.
 *
 * Returns:
 *  - 0 on success, non-zero on basic input validation failure.
 */
int cybo_compute_workload_residual(
    double energy_req_J,
    double energy_corridor_max_J,
    double hydraulic_load,
    double hydraulic_corridor_max,
    double carbon_intensity,
    double carbon_corridor_max,
    double uncertainty_raw,
    double vt_before_hint,
    CyboWorkloadRiskCoords *out_risks,
    CyboWorkloadResidual *out_residual
) {
    if (out_risks == nullptr || out_residual == nullptr) {
        return 1;
    }
    if (energy_req_J < 0.0 || energy_corridor_max_J <= 0.0 ||
        hydraulic_corridor_max <= 0.0 || carbon_corridor_max <= 0.0) {
        return 2;
    }

    CyboWorkloadRiskCoords risks{};
    risks.r_energy      = normalize_energy(energy_req_J, energy_corridor_max_J);
    risks.r_hydraulics  = normalize_hydraulics(hydraulic_load, hydraulic_corridor_max);
    risks.r_carbon      = normalize_carbon(carbon_intensity, carbon_corridor_max);
    risks.r_uncertainty = normalize_uncertainty(uncertainty_raw);

    // Weights chosen to emphasize carbon and hydraulics, consistent with eco-priority.
    const double w_e = 0.2;
    const double w_h = 0.3;
    const double w_c = 0.4;
    const double w_u = 0.1;

    double vt_before = vt_before_hint;
    if (vt_before < 0.0) {
        vt_before = compute_vt(risks, w_e, w_h, w_c, w_u);
    }
    double vt_after = compute_vt(risks, w_e, w_h, w_c, w_u);
    double delta_vt = vt_after - vt_before;

    out_risks->r_energy      = risks.r_energy;
    out_risks->r_hydraulics  = risks.r_hydraulics;
    out_risks->r_carbon      = risks.r_carbon;
    out_risks->r_uncertainty = risks.r_uncertainty;

    out_residual->vt_before = vt_before;
    out_residual->vt_after  = vt_after;
    out_residual->delta_vt  = delta_vt;

    return 0;
}

} // extern "C"
