// filename: crates/prometheus_praxis_ai/src/ffi/workload.rs

#![allow(unsafe_code)]

use std::os::raw::{c_double, c_int, c_size_t};

use crate::{ResidualSlice, WorkloadRiskCoords};

/// C-compatible POD struct for workload residuals returned by C++.
#[repr(C)]
pub struct CWorkloadResidual {
    pub energyreqJ: c_double,
    pub delta_vt: c_double,
}

/// C-compatible POD struct mirroring workload risk coordinates.
#[repr(C)]
pub struct CWorkloadRiskCoords {
    pub r_energy: c_double,
    pub r_hydraulics: c_double,
    pub r_uncertainty: c_double,
}

extern "C" {
    /// eco_engine_workload numeric kernel:
    ///
    /// int compute_workload_residual(
    ///     const double* input_data,
    ///     size_t data_size,
    ///     CWorkloadRiskCoords* out_risks,
    ///     CWorkloadResidual* out_residual
    /// );
    ///
    /// Returns 0 on success, non-zero on failure.
    fn compute_workload_residual_cpp(
        input_data: *const c_double,
        data_size: c_size_t,
        out_risks: *mut CWorkloadRiskCoords,
        out_residual: *mut CWorkloadResidual,
    ) -> c_int;
}

/// Safe wrapper around the C++ workload kernel.
///
/// Input slice should contain parameters in the agreed order:
/// [energyreqJ, energysurplusJ, hydraulicrisk, uncertaintyrisk].
pub fn call_workload_kernel(
    input_values: &[f64],
) -> Result<(WorkloadRiskCoords, ResidualSlice), crate::engine::EngineError> {
    if input_values.len() < 4 {
        return Err(crate::engine::EngineError::InvalidInput(
            "input_values must contain at least 4 elements".to_string(),
        ));
    }

    let mut c_risks = CWorkloadRiskCoords {
        r_energy: 0.0,
        r_hydraulics: 0.0,
        r_uncertainty: 0.0,
    };
    let mut c_residual = CWorkloadResidual {
        energyreqJ: 0.0,
        delta_vt: 0.0,
    };

    let status = unsafe {
        compute_workload_residual_cpp(
            input_values.as_ptr() as *const c_double,
            input_values.len() as c_size_t,
            &mut c_risks as *mut CWorkloadRiskCoords,
            &mut c_residual as *mut CWorkloadResidual,
        )
    };

    if status != 0 {
        return Err(crate::engine::EngineError::KernelFailure(
            "compute_workload_residual_cpp returned non-zero status".to_string(),
        ));
    }

    let risks = WorkloadRiskCoords {
        r_energy: c_risks.r_energy as f64,
        r_hydraulics: c_risks.r_hydraulics as f64,
        r_uncertainty: c_risks.r_uncertainty as f64,
    }
    .clamped();

    let residual = ResidualSlice::new(0.0, c_residual.delta_vt as f64);

    Ok((risks, residual))
}
