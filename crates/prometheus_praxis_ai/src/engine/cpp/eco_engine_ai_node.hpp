// filename: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_ai_node.hpp

#ifndef ECO_ENGINE_AI_NODE_HPP
#define ECO_ENGINE_AI_NODE_HPP

extern "C" {

/// C-compatible POD struct for AI node risk coordinates.
/// Must match crates/prometheus_praxis_ai/src/ffi/ai_node.rs.
typedef struct {
    double r_energy_compute;
    double r_cooling_water;
    double r_carbon;
    double r_biodiversity;
    double r_uncertainty;
} CAiNodeRiskCoords;

/// C-compatible POD struct for AI node residual slice.
/// Must match crates/prometheus_praxis_ai/src/ffi/ai_node.rs.
typedef struct {
    double vt_before_ai;
    double vt_after_ai;
    double delta_vt_ai;
} CAiNodeResidualSlice;

/// AI node energetics kernel.
///
/// Inputs:
/// - pue, cue: efficiency metrics.
/// - power_kw, cooling_kw, thermal_output_kw: instantaneous draws and waste heat.
/// - carbon_intensity, biodiversity_risk, uncertainty_risk: normalized [0,1] scores.
///
/// Outputs:
/// - out_risks: normalized risk coordinates.
/// - out_residual: residual slice vt_ai using ALN AiDatacenterNode2026v1 weights.
///
/// Returns 0 on success, non-zero on failure. Non-actuating.
int compute_ai_node_residual(
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
);

} // extern "C"

#endif // ECO_ENGINE_AI_NODE_HPP
