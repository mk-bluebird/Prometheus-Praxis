// FILE: crates/bioscale-fairness-validator/src/lib.rs
// DESTINATION: crates/bioscale-fairness-validator/src/lib.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard

#![forbid(unsafe_code)]
#![warn(missing_docs)]

//! Bioscale fairness validator for MT6883 healthcare corridors.
//!
//! Non-actuating fairness checks over K/E/R and Lyapunov residual,
//! using the shared `kerresidual` spine.

use kerresidual::{
    check_safe_step, compute_e, compute_k, compute_r, compute_residual, KerSnapshot,
    PlaneWeights, RiskVector,
};
use serde::{Deserialize, Serialize};

/// Fairness input captures before/after states for a single step.
///
/// All K/E/R values are raw; they are clamped by the `kerresidual` helpers.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FairnessInput {
    /// Risk vector before the step.
    pub risk_vector_before: RiskVector,
    /// Risk vector after the step.
    pub risk_vector_after: RiskVector,
    /// Plane weights used for residual computation.
    pub weights: PlaneWeights,
    /// Raw K value before the step.
    pub k_before: f32,
    /// Raw K value after the step.
    pub k_after: f32,
    /// Raw E value before the step.
    pub e_before: f32,
    /// Raw E value after the step.
    pub e_after: f32,
    /// Raw R value before the step.
    pub r_before: f32,
    /// Raw R value after the step.
    pub r_after: f32,
}

/// Verdict over fairness conditions for a single step.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FairnessVerdict {
    /// Lyapunov residual step is within the allowed safe step bound.
    pub safe_step_ok: bool,
    /// K is monotone non-decreasing.
    pub k_monotone_ok: bool,
    /// E is monotone non-decreasing.
    pub e_monotone_ok: bool,
    /// R is monotone non-increasing.
    pub r_monotone_ok: bool,
}

impl FairnessVerdict {
    /// True if all fairness predicates pass.
    pub fn all_ok(&self) -> bool {
        self.safe_step_ok && self.k_monotone_ok && self.e_monotone_ok && self.r_monotone_ok
    }
}

/// Evaluate fairness for a given step.
///
/// Conditions:
/// - Lyapunov residual must not increase beyond `epsilon`.
/// - K and E must be monotone non-decreasing (after clamping).
/// - R must be monotone non-increasing (after clamping).
pub fn evaluate_fairness(input: &FairnessInput, epsilon: f32) -> FairnessVerdict {
    let v_before = compute_residual(&input.weights, &input.risk_vector_before);
    let v_after = compute_residual(&input.weights, &input.risk_vector_after);
    let safe_step_ok = check_safe_step(v_before, v_after, epsilon);

    let k_before = compute_k(input.k_before);
    let k_after = compute_k(input.k_after);
    let e_before = compute_e(input.e_before);
    let e_after = compute_e(input.e_after);
    let r_before = compute_r(input.r_before);
    let r_after = compute_r(input.r_after);

    let k_monotone_ok = k_after >= k_before;
    let e_monotone_ok = e_after >= e_before;
    let r_monotone_ok = r_after <= r_before;

    FairnessVerdict {
        safe_step_ok,
        k_monotone_ok,
        e_monotone_ok,
        r_monotone_ok,
    }
}

/// Derive a `KerSnapshot` from raw values and residual.
///
/// Helper for tests and offline checks.
pub fn snapshot_from_values(k: f32, e: f32, r: f32, vt: f32) -> KerSnapshot {
    KerSnapshot {
        k: compute_k(k),
        e: compute_e(e),
        r: compute_r(r),
        vt,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fairness_accepts_monotone_improvement() {
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

        let before = RiskVector {
            renergy: 0.2,
            rhydraulic: 0.2,
            rbiology: 0.2,
            rcarbon: 0.2,
            rmaterials: 0.2,
            rbiodiversity: 0.2,
            rdataquality: 0.2,
            rtopology: 0.2,
        };

        let after = RiskVector {
            renergy: 0.1,
            rhydraulic: 0.1,
            rbiology: 0.1,
            rcarbon: 0.1,
            rmaterials: 0.1,
            rbiodiversity: 0.1,
            rdataquality: 0.1,
            rtopology: 0.1,
        };

        let input = FairnessInput {
            risk_vector_before: before,
            risk_vector_after: after,
            weights,
            k_before: 0.5,
            k_after: 0.6,
            e_before: 0.5,
            e_after: 0.7,
            r_before: 0.3,
            r_after: 0.2,
        };

        let verdict = evaluate_fairness(&input, 1.0e-6);
        assert!(verdict.all_ok());
    }

    #[test]
    fn fairness_rejects_residual_increase() {
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

        let before = RiskVector {
            renergy: 0.1,
            rhydraulic: 0.1,
            rbiology: 0.1,
            rcarbon: 0.1,
            rmaterials: 0.1,
            rbiodiversity: 0.1,
            rdataquality: 0.1,
            rtopology: 0.1,
        };

        let after = RiskVector {
            renergy: 0.3,
            rhydraulic: 0.3,
            rbiology: 0.3,
            rcarbon: 0.3,
            rmaterials: 0.3,
            rbiodiversity: 0.3,
            rdataquality: 0.3,
            rtopology: 0.3,
        };

        let input = FairnessInput {
            risk_vector_before: before,
            risk_vector_after: after,
            weights,
            k_before: 0.6,
            k_after: 0.6,
            e_before: 0.7,
            e_after: 0.7,
            r_before: 0.2,
            r_after: 0.2,
        };

        let verdict = evaluate_fairness(&input, 0.0);
        assert!(!verdict.safe_step_ok);
    }
}
