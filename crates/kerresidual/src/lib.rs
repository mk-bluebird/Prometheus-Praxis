// filename: src/lib.rs
// destination: eco_restoration_shard/crates/kerresidual/src/lib.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

#![forbid(unsafe_code)]
#![warn(missing_docs)]

//! KER residual math spine.
//!
//! Shared, non-actuating implementation of Lyapunov residual computation,
//! K/E/R clamping, and safe-step checks. All higher-level crates should
//! reuse this spine instead of re-implementing the math.

use serde::{Deserialize, Serialize};

/// Risk vector with per-plane normalized risk coordinates.
///
/// All coordinates are expected in the interval [0, 1].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RiskVector {
    pub renergy: f32,
    pub rhydraulic: f32,
    pub rbiology: f32,
    pub rcarbon: f32,
    pub rmaterials: f32,
    pub rbiodiversity: f32,
    pub rdataquality: f32,
    pub rtopology: f32,
}

/// Plane weights used in residual computation.
///
/// All weights must be non-negative. Non-offsettable plane semantics are
/// enforced at higher layers via corridor and plane contracts.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeights {
    pub wenergy: f32,
    pub whydraulic: f32,
    pub wbiology: f32,
    pub wcarbon: f32,
    pub wmaterials: f32,
    pub wbiodiversity: f32,
    pub wdataquality: f32,
    pub wtopology: f32,
}

/// Snapshot of KER values and residual for a shard window.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerSnapshot {
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub vt: f32,
}

/// Compute Lyapunov residual Vt = sum_j w_j * r_j^2.
///
/// Inputs are purely numeric; the caller is responsible for normalization.
pub fn compute_residual(weights: &PlaneWeights, rv: &RiskVector) -> f32 {
    weights.wenergy * rv.renergy * rv.renergy
        + weights.whydraulic * rv.rhydraulic * rv.rhydraulic
        + weights.wbiology * rv.rbiology * rv.rbiology
        + weights.wcarbon * rv.rcarbon * rv.rcarbon
        + weights.wmaterials * rv.rmaterials * rv.rmaterials
        + weights.wbiodiversity * rv.rbiodiversity * rv.rbiodiversity
        + weights.wdataquality * rv.rdataquality * rv.rdataquality
        + weights.wtopology * rv.rtopology * rv.rtopology
}

/// Clamp K into [0, 1].
pub fn compute_k(k: f32) -> f32 {
    k.clamp(0.0, 1.0)
}

/// Clamp E into [0, 1].
pub fn compute_e(e: f32) -> f32 {
    e.clamp(0.0, 1.0)
}

/// Clamp R into [0, 1].
pub fn compute_r(r: f32) -> f32 {
    r.clamp(0.0, 1.0)
}

/// Check that the safe-step condition holds: vt_after <= vt_before + epsilon.
///
/// This is suitable for CI and runtime lane guards.
pub fn check_safe_step(vt_before: f32, vt_after: f32, epsilon: f32) -> bool {
    vt_after <= vt_before + epsilon
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn residual_non_negative() {
        let weights = PlaneWeights {
            wenergy: 1.0,
            whydraulic: 1.0,
            wbiology: 1.0,
            wcarbon: 1.0,
            wmaterials: 1.0,
            wbiodiversity: 1.0,
            wdataquality: 1.0,
            wtopology: 1.0,
        };
        let risk = RiskVector {
            renergy: 0.1,
            rhydraulic: 0.2,
            rbiology: 0.3,
            rcarbon: 0.4,
            rmaterials: 0.5,
            rbiodiversity: 0.6,
            rdataquality: 0.7,
            rtopology: 0.8,
        };
        let vt = compute_residual(&weights, &risk);
        assert!(vt >= 0.0);
    }

    #[test]
    fn safe_step_with_zero_epsilon() {
        assert!(check_safe_step(1.0, 1.0, 0.0));
        assert!(!check_safe_step(1.0, 1.0001, 0.0));
    }

    #[test]
    fn safe_step_with_positive_epsilon() {
        assert!(check_safe_step(1.0, 1.05, 0.1));
        assert!(!check_safe_step(1.0, 1.2, 0.1));
    }

    #[test]
    fn ker_clamping() {
        assert_eq!(compute_k(-0.1), 0.0);
        assert_eq!(compute_k(0.5), 0.5);
        assert_eq!(compute_k(1.1), 1.0);

        assert_eq!(compute_e(-0.1), 0.0);
        assert_eq!(compute_e(0.5), 0.5);
        assert_eq!(compute_e(1.1), 1.0);

        assert_eq!(compute_r(-0.1), 0.0);
        assert_eq!(compute_r(0.5), 0.5);
        assert_eq!(compute_r(1.1), 1.0);
    }
}
