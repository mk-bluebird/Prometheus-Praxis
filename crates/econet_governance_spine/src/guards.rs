// Filename: crates/econet_governance_spine/src/guards.rs
// Destination: crates/econet_governance_spine/src/guards.rs

#![forbid(unsafe_code)]

use crate::{BlastRadius, KerResidual, LaneStatus, PlaneWeight};

#[derive(Debug, Clone, Copy)]
pub enum LaneFilter {
    ExactProd,
    ExactExpProd,
}

#[derive(Debug, Clone)]
pub struct KerGuardInputs {
    pub old_k: f64,
    pub old_e: f64,
    pub old_r: f64,
    pub new_k: f64,
    pub new_e: f64,
    pub new_r: f64,
}

#[derive(Debug, Clone)]
pub struct KerGuardResult {
    pub ok: bool,
    pub reason: Option<String>,
}

#[derive(Debug, Clone)]
pub struct LaneGuardInputs {
    pub lane_status: LaneStatus,
    pub filter: LaneFilter,
    pub now_utc: i64,
}

#[derive(Debug, Clone)]
pub struct LaneGuardResult {
    pub admissible: bool,
    pub reason: Option<String>,
}

#[derive(Debug, Clone)]
pub struct Mt6883GuardInputs {
    pub ker: KerResidual,
    pub lane: LaneStatus,
    pub blast: BlastRadius,
    pub plane_weights: Vec<PlaneWeight>,
}

#[derive(Debug, Clone)]
pub struct Mt6883GuardResult {
    pub allowed: bool,
    pub reason: Option<String>,
}

#[derive(Debug, Clone)]
pub struct KerUpgradeGuard;

impl KerUpgradeGuard {
    pub fn check(inputs: KerGuardInputs) -> KerGuardResult {
        if inputs.new_k < inputs.old_k {
            return KerGuardResult {
                ok: false,
                reason: Some(format!(
                    "new K {} is less than old K {}",
                    inputs.new_k, inputs.old_k
                )),
            };
        }
        if inputs.new_e < inputs.old_e {
            return KerGuardResult {
                ok: false,
                reason: Some(format!(
                    "new E {} is less than old E {}",
                    inputs.new_e, inputs.old_e
                )),
            };
        }
        if inputs.new_r > inputs.old_r {
            return KerGuardResult {
                ok: false,
                reason: Some(format!(
                    "new R {} is greater than old R {}",
                    inputs.new_r, inputs.old_r
                )),
            };
        }
        KerGuardResult { ok: true, reason: None }
    }
}

pub struct LaneGuard;

impl LaneGuard {
    pub fn check(inputs: LaneGuardInputs) -> LaneGuardResult {
        let lane = inputs.lane_status.lane.as_str();
        let expired = inputs.now_utc > inputs.lane_status.expires_utc;
        if expired {
            return LaneGuardResult {
                admissible: false,
                reason: Some("lane verdict is stale".to_string()),
            };
        }

        match inputs.filter {
            LaneFilter::ExactProd => {
                if lane != "PROD" {
                    return LaneGuardResult {
                        admissible: false,
                        reason: Some(format!("lane '{}' is not PROD", lane)),
                    };
                }
            }
            LaneFilter::ExactExpProd => {
                if lane != "EXPPROD" {
                    return LaneGuardResult {
                        admissible: false,
                        reason: Some(format!("lane '{}' is not EXPPROD", lane)),
                    };
                }
            }
        }

        if !inputs.lane_status.carbon_negative_ok {
            return LaneGuardResult {
                admissible: false,
                reason: Some("carbonnegativeok is false".to_string()),
            };
        }

        if !inputs.lane_status.restoration_ok {
            return LaneGuardResult {
                admissible: false,
                reason: Some("restorationok is false".to_string()),
            };
        }

        LaneGuardResult {
            admissible: true,
            reason: None,
        }
    }
}

pub struct Mt6883Guard;

impl Mt6883Guard {
    pub fn check(inputs: Mt6883GuardInputs) -> Mt6883GuardResult {
        if inputs.ker.k < 0.90 {
            return Mt6883GuardResult {
                allowed: false,
                reason: Some("K below 0.90".to_string()),
            };
        }
        if inputs.ker.e < 0.90 {
            return Mt6883GuardResult {
                allowed: false,
                reason: Some("E below 0.90".to_string()),
            };
        }
        if inputs.ker.r > 0.13 {
            return Mt6883GuardResult {
                allowed: false,
                reason: Some("R above 0.13".to_string()),
            };
        }

        if inputs.blast.radius_meters > 0.0 && inputs.blast.adjacency_count > 0 {
            if inputs.ker.r > 0.10 {
                return Mt6883GuardResult {
                    allowed: false,
                    reason: Some("blast radius too large for residual R".to_string()),
                };
            }
        }

        let non_offsettable_violation = inputs
            .plane_weights
            .iter()
            .any(|p| p.non_offsettable && p.weight <= 0.0);

        if non_offsettable_violation {
            return Mt6883GuardResult {
                allowed: false,
                reason: Some("non-offsettable plane weight invalid".to_string()),
            };
        }

        Mt6883GuardResult {
            allowed: true,
            reason: None,
        }
    }
}
