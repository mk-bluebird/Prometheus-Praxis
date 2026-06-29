// Filename: crates/ecosafety-nanoswarm-urban-core/src/roh_mpc_guard.rs

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LaneRoHProfile {
    pub lane_id: String,
    /// RoH over horizon steps: r_i(k) in [0,1].
    pub roh_horizon: Vec<f64>,
    /// Governance weight w_i >= 0.
    pub weight: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RohGlobalConstraint {
    pub roh_ceiling: f64, // e.g. 0.30
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RohGuardResult {
    pub allow: bool,
    pub violating_step: Option<usize>,
    pub roh_global_max: f64,
}

pub fn evaluate_global_roh(
    profiles: &[LaneRoHProfile],
    constraint: &RohGlobalConstraint,
) -> RohGuardResult {
    let horizon_len = profiles
        .iter()
        .map(|p| p.roh_horizon.len())
        .max()
        .unwrap_or(0);

    let mut roh_global_max = 0.0;
    let mut violating_step = None;

    for k in 0..horizon_len {
        let mut sum = 0.0;
        for p in profiles {
            if let Some(r) = p.roh_horizon.get(k) {
                sum += p.weight * (*r);
            }
        }
        if sum > roh_global_max {
            roh_global_max = sum;
        }
        if sum > constraint.roh_ceiling && violating_step.is_none() {
            violating_step = Some(k);
        }
    }

    RohGuardResult {
        allow: violating_step.is_none(),
        violating_step,
        roh_global_max,
    }
}
