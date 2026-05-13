#![forbid(unsafe_code)]

use crate::plane_weights::PlaneWeights;
use crate::types::{KerSnapshot, RiskCoords};
use thiserror::Error;

/// Snapshot of KER metrics before/after a transition.
#[derive(Debug, Clone)]
pub struct KerSnapshot {
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub lane: String,
}

/// Per-plane risk coordinates needed for invariants.
#[derive(Debug, Clone)]
pub struct RiskCoords {
    pub r_carbon_prev: f64,
    pub r_carbon_next: f64,
    pub r_biodiv_prev: f64,
    pub r_biodiv_next: f64,
    pub r_calib_prev: f64,
    pub r_calib_next: f64,
    pub r_sigma_prev: f64,
    pub r_sigma_next: f64,
}

/// Plane weights + invariant flags for a region/node family.
#[derive(Debug, Clone)]
pub struct PlaneWeights {
    pub carbon_nonoffsettable: bool,
    pub biodiversity_nonoffsettable: bool,
    pub r_carbon_soft: f64,
    pub r_biodiv_soft: f64,
    pub w_carbon: f64,
    pub w_materials: f64,
    pub w_biodiv: f64,
    pub w_hydro: f64,
    pub w_data: f64,
    pub w_topology: f64,
    pub w_underspec: f64,
}

/// Types of invariant failures for diagnostics and CI.
#[derive(Debug, Error)]
pub enum InvariantError {
    #[error("non-compensation violation on plane {plane_id}")]
    NonCompensationViolation { plane_id: String },

    #[error("uncertainty monotonicity violation for coord {coord_id}")]
    UncertaintyViolation { coord_id: String },
}

/// Check non-compensation for carbon and biodiversity based on PlaneWeights.
pub fn check_plane_noncompensation(
    prev: &KerSnapshot,
    next: &KerSnapshot,
    risks: &RiskCoords,
    weights: &PlaneWeights,
) -> Result<(), InvariantError> {
    // Only enforce on certain lanes; extend if needed.
    let lane = prev.lane.as_str();
    if lane != "EXPPROD" && lane != "PROD" {
        return Ok(());
    }

    if weights.carbon_nonoffsettable {
        if risks.r_carbon_next > risks.r_carbon_prev
            && risks.r_carbon_prev >= weights.r_carbon_soft
        {
            if next.e > prev.e || next.k > prev.k || next.r < prev.r {
                return Err(InvariantError::NonCompensationViolation {
                    plane_id: "carbon".to_string(),
                });
            }
        }
    }

    if weights.biodiversity_nonoffsettable {
        if risks.r_biodiv_next > risks.r_biodiv_prev
            && risks.r_biodiv_prev >= weights.r_biodiv_soft
        {
            if next.e > prev.e || next.k > prev.k || next.r < prev.r {
                return Err(InvariantError::NonCompensationViolation {
                    plane_id: "biodiversity".to_string(),
                });
            }
        }
    }

    Ok(())
}

/// Check uncertainty monotonicity for rcalib and rsigma.
pub fn check_uncertainty_monotonicity(
    prev: &KerSnapshot,
    next: &KerSnapshot,
    risks: &RiskCoords,
) -> Result<(), InvariantError> {
    // These invariants can apply in all lanes; adjust if you want to restrict.
    if risks.r_calib_next > risks.r_calib_prev {
        if next.e > prev.e || next.k > prev.k || next.r < prev.r {
            return Err(InvariantError::UncertaintyViolation {
                coord_id: "rcalib".to_string(),
            });
        }
    }

    if risks.r_sigma_next > risks.r_sigma_prev {
        if next.e > prev.e || next.k > prev.k || next.r < prev.r {
            return Err(InvariantError::UncertaintyViolation {
                coord_id: "rsigma".to_string(),
            });
        }
    }

    Ok(())
}

/// Compute scalar Lyapunov residual V_t from per-plane risks and weights.
pub fn compute_vt(residuals: &PerPlaneResiduals, weights: &PlaneWeights) -> f64 {
    let mut vt = 0.0;
    vt += weights.w_carbon * residuals.r_carbon.powi(2);
    vt += weights.w_materials * residuals.r_materials.powi(2);
    vt += weights.w_biodiv * residuals.r_biodiv.powi(2);
    vt += weights.w_hydro * residuals.r_hydro.powi(2);
    vt += weights.w_data * residuals.r_data.powi(2);
    vt += weights.w_topology * residuals.r_topology.powi(2);
    vt += weights.w_underspec * residuals.r_underspec.powi(2);
    vt
}

/// Normalized residualscore in [0,1] from V_t and a chosen reference V_ref.
pub fn compute_residualscore(vt: f64, v_ref: f64) -> f64 {
    if v_ref <= 0.0 {
        return 1.0;
    }
    let ratio = vt / v_ref;
    if ratio >= 1.0 {
        1.0
    } else if ratio <= 0.0 {
        0.0
    } else {
        ratio
    }
}

/// Per-plane residual bundle.
#[derive(Debug, Clone)]
pub struct PerPlaneResiduals {
    pub r_carbon: f64,
    pub r_materials: f64,
    pub r_biodiv: f64,
    pub r_hydro: f64,
    pub r_data: f64,
    pub r_topology: f64,
    pub r_underspec: f64,
}
