// filename: crates/prometheus_praxis_ai/src/ffi/ai_node.rs

#![allow(unsafe_code)]

use std::os::raw::{c_double, c_int};

use crate::{AiNodeRiskCoords, ResidualSlice};

/// C-compatible POD struct for AI node risk coordinates from C++.
#[repr(C)]
pub struct CAiNodeRiskCoords {
    pub r_energy_compute: c_double,
    pub r_cooling_water: c_double,
    pub r_carbon: c_double,
    pub r_biodiversity: c_double,
    pub r_uncertainty: c_double,
}

/// C-compatible POD struct for AI node residual slice.
#[repr(C)]
pub struct CAiNodeResidualSlice {
    pub vt_before_ai: c_double,
    pub vt_after_ai: c_double,
    pub delta_vt_ai: c_double,
}

extern "C" {
    /// eco_engine_ai_node numeric kernel:
    ///
    /// int compute_ai_node_residual(
    ///     double pue,
    ///     double cue,
    ///     double power_kw,
    ///     double cooling_kw,
    ///     double thermal_output_kw,
    ///     double carbon_intensity,
    ///     double biodiversity_risk,
    ///     double uncertainty_risk,
    ///     CAiNodeRiskCoords* out_risks,
    ///     CAiNodeResidualSlice* out_residual
    /// );
    ///
    /// Returns 0 on success, non-zero on failure.
    fn compute_ai_node_residual_cpp(
        pue: c_double,
        cue: c_double,
        power_kw: c_double,
        cooling_kw: c_double,
        thermal_output_kw: c_double,
        carbon_intensity: c_double,
        biodiversity_risk: c_double,
        uncertainty_risk: c_double,
        out_risks: *mut CAiNodeRiskCoords,
        out_residual: *mut CAiNodeResidualSlice,
    ) -> c_int;
}

/// Safe wrapper around the C++ AI node kernel.
pub fn call_ai_node_kernel(
    pue: f64,
    cue: f64,
    power_kw: f64,
    cooling_kw: f64,
    thermal_output_kw: f64,
    carbon_intensity: f64,
    biodiversity_risk: f64,
    uncertainty_risk: f64,
) -> Result<(AiNodeRiskCoords, ResidualSlice), crate::engine::EngineError> {
    let mut c_risks = CAiNodeRiskCoords {
        r_energy_compute: 0.0,
        r_cooling_water: 0.0,
        r_carbon: 0.0,
        r_biodiversity: 0.0,
        r_uncertainty: 0.0,
    };
    let mut c_residual = CAiNodeResidualSlice {
        vt_before_ai: 0.0,
        vt_after_ai: 0.0,
        delta_vt_ai: 0.0,
    };

    let status = unsafe {
        compute_ai_node_residual_cpp(
            pue,
            cue,
            power_kw,
            cooling_kw,
            thermal_output_kw,
            carbon_intensity,
            biodiversity_risk,
            uncertainty_risk,
            &mut c_risks as *mut CAiNodeRiskCoords,
            &mut c_residual as *mut CAiNodeResidualSlice,
        )
    };

    if status != 0 {
        return Err(crate::engine::EngineError::KernelFailure(
            "compute_ai_node_residual_cpp returned non-zero status".to_string(),
        ));
    }

    let risks = AiNodeRiskCoords {
        r_energy_compute: c_risks.r_energy_compute as f64,
        r_cooling_water: c_risks.r_cooling_water as f64,
        r_carbon: c_risks.r_carbon as f64,
        r_biodiversity: c_risks.r_biodiversity as f64,
        r_uncertainty: c_risks.r_uncertainty as f64,
    }
    .clamped();

    let residual = ResidualSlice::new(
        c_residual.vt_before_ai as f64,
        c_residual.vt_after_ai as f64,
    );

    Ok((risks, residual))
}
