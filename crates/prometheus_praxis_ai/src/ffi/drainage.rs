// filename: crates/prometheus_praxis_ai/src/ffi/drainage.rs

#![allow(unsafe_code)]

use std::os::raw::{c_double, c_int, c_size_t};

use crate::{DrainageRiskCoords, ResidualSlice};

/// C-compatible POD struct mirroring the drainage risk coordinates
/// produced by the C++ eco_engine_drainage kernel.
#[repr(C)]
pub struct CDrainageRiskCoords {
    pub r_bod: c_double,
    pub r_tss: c_double,
    pub r_cec: c_double,
    pub r_hydraulics: c_double,
    pub r_uncertainty: c_double,
}

/// C-compatible POD struct for a residual slice from the drainage kernel.
#[repr(C)]
pub struct CResidualSlice {
    pub vt_before: c_double,
    pub vt_after: c_double,
    pub delta_vt: c_double,
}

extern "C" {
    /// eco_engine_drainage numeric kernel:
    ///
    /// int compute_drainage_decay(
    ///     double bod_mg_l,
    ///     double tss_mg_l,
    ///     double cec_cmol_per_kg,
    ///     double flow_rate_m3s,
    ///     double water_temp_c,
    ///     double elevation_m,
    ///     CDrainageRiskCoords* out_risks,
    ///     CResidualSlice* out_residual
    /// );
    ///
    /// Returns 0 on success, non-zero on failure.
    fn compute_drainage_decay(
        bod_mg_l: c_double,
        tss_mg_l: c_double,
        cec_cmol_per_kg: c_double,
        flow_rate_m3s: c_double,
        water_temp_c: c_double,
        elevation_m: c_double,
        out_risks: *mut CDrainageRiskCoords,
        out_residual: *mut CResidualSlice,
    ) -> c_int;
}

/// Safe wrapper around the C++ drainage kernel.
///
/// This function is internal; the public API is in src/engine/mod.rs.
/// It marshals Rust types into C, calls the unsafe function, and
/// converts results back into Rust types.
pub fn call_drainage_kernel(
    bod_mg_l: f64,
    tss_mg_l: f64,
    cec_cmol_per_kg: f64,
    flow_rate_m3s: f64,
    water_temp_c: f64,
    elevation_m: f64,
) -> Result<(DrainageRiskCoords, ResidualSlice), crate::engine::EngineError> {
    let mut c_risks = CDrainageRiskCoords {
        r_bod: 0.0,
        r_tss: 0.0,
        r_cec: 0.0,
        r_hydraulics: 0.0,
        r_uncertainty: 0.0,
    };
    let mut c_residual = CResidualSlice {
        vt_before: 0.0,
        vt_after: 0.0,
        delta_vt: 0.0,
    };

    let status = unsafe {
        compute_drainage_decay(
            bod_mg_l,
            tss_mg_l,
            cec_cmol_per_kg,
            flow_rate_m3s,
            water_temp_c,
            elevation_m,
            &mut c_risks as *mut CDrainageRiskCoords,
            &mut c_residual as *mut CResidualSlice,
        )
    };

    if status != 0 {
        return Err(crate::engine::EngineError::KernelFailure(
            "compute_drainage_decay returned non-zero status".to_string(),
        ));
    }

    let risks = DrainageRiskCoords {
        r_bod: c_risks.r_bod as f64,
        r_tss: c_risks.r_tss as f64,
        r_cec: c_risks.r_cec as f64,
        r_hydraulics: c_risks.r_hydraulics as f64,
        r_uncertainty: c_risks.r_uncertainty as f64,
    }
    .clamped();

    let residual = ResidualSlice::new(
        c_residual.vt_before as f64,
        c_residual.vt_after as f64,
    );

    Ok((risks, residual))
}
