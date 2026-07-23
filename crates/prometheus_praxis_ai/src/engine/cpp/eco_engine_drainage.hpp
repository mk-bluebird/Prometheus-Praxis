// filename: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_drainage.hpp

#ifndef ECO_ENGINE_DRAINAGE_HPP
#define ECO_ENGINE_DRAINAGE_HPP

extern "C" {

/// C-compatible POD struct mirroring the drainage risk coordinates.
/// This must stay in sync with crates/prometheus_praxis_ai/src/ffi/drainage.rs
typedef struct {
    double r_bod;
    double r_tss;
    double r_cec;
    double r_hydraulics;
    double r_uncertainty;
} CDrainageRiskCoords;

/// C-compatible POD struct for a residual slice from the drainage kernel.
/// This must stay in sync with crates/prometheus_praxis_ai/src/ffi/drainage.rs
typedef struct {
    double vt_before;
    double vt_after;
    double delta_vt;
} CResidualSlice;

/// High-performance hydraulics / drainage decay kernel.
///
/// Inputs:
/// - bod_mg_l: Biochemical Oxygen Demand [mg/L], expected in [0, 80].
/// - tss_mg_l: Total Suspended Solids [mg/L], expected in [0, 500].
/// - cec_cmol_per_kg: Cation Exchange Capacity [cmol(+)/kg], expected in [0, 50].
/// - flow_rate_m3s: Canal flow rate [m3/s], >= 0.
/// - water_temp_c: Water temperature [°C], expected in [0, 45].
/// - elevation_m: Elevation [m], expected in [-100, 2000].
///
/// Outputs:
/// - out_risks: normalized risk coordinates in [0,1].
/// - out_residual: residual slice (vt_before, vt_after, delta_vt) using
///   ALN DrainageDecayKernel2026v1 weights.
///
/// Returns 0 on success, non-zero on failure. This function is non-actuating.
int compute_drainage_decay(
    double bod_mg_l,
    double tss_mg_l,
    double cec_cmol_per_kg,
    double flow_rate_m3s,
    double water_temp_c,
    double elevation_m,
    CDrainageRiskCoords* out_risks,
    CResidualSlice* out_residual
);

} // extern "C"

#endif // ECO_ENGINE_DRAINAGE_HPP
