// filename: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_workload.hpp

#ifndef ECO_ENGINE_WORKLOAD_HPP
#define ECO_ENGINE_WORKLOAD_HPP

#include <stddef.h>

extern "C" {

/// C-compatible POD struct for workload residuals.
/// Must match crates/prometheus_praxis_ai/src/ffi/workload.rs.
typedef struct {
    double energyreqJ;
    double delta_vt;
} CWorkloadResidual;

/// C-compatible POD struct for workload risk coordinates.
/// Must match crates/prometheus_praxis_ai/src/ffi/workload.rs.
typedef struct {
    double r_energy;
    double r_hydraulics;
    double r_uncertainty;
} CWorkloadRiskCoords;

/// Workload energetics kernel.
///
/// Input data array must contain, in order:
/// [energyreqJ, energysurplusJ, hydraulicrisk, uncertaintyrisk].
///
/// Outputs:
/// - out_risks: normalized risk coordinates.
/// - out_residual: residual slice with delta_vt consistent with ALN workload kernel.
///
/// Returns 0 on success, non-zero on failure. Non-actuating.
int compute_workload_residual(
    const double* input_data,
    size_t data_size,
    CWorkloadRiskCoords* out_risks,
    CWorkloadResidual* out_residual
);

} // extern "C"

#endif // ECO_ENGINE_WORKLOAD_HPP
