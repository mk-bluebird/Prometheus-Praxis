// filename: crates/kerresidual/src/lib.rs
// License: MIT OR Apache-2-0
// Rust edition: 2024
// rust-version = "1.85"

#![forbid(unsafe_code)]
#![warn(missing_docs)]

use serde::{Deserialize, Serialize};

/// Raw biodegradation test data under ISO 14851/14855.
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct BiodegradationTest {
    /// Mean biodegradation percentage B at test horizon Th, corrected for blanks and reference.
    pub b_mean_percent: f32,
    /// Standard deviation across replicates.
    pub b_std_percent: f32,
    /// Test horizon in days (for example, 28).
    pub th_days: f32,
}

/// Parameters loaded from ALN shard `biodegradation-microresidue-link.v1.aln`.
#[derive(Debug, Clone, Copy)]
pub struct BiodegradationMapParams {
    /// Upper corridor threshold for high biodegradation (safe band).
    pub b_safe_high: f32,
    /// Lower corridor threshold for low biodegradation (hard band).
    pub b_hard_low: f32,
    /// Maximum expected standard deviation for stable tests.
    pub b_sigma_max: f32,
}

/// Compute a 0-1 microresidue risk coordinate r_micro from biodegradation data.
///
/// r_micro = 1 - g(B_eff), where B_eff is a conservative effective biodegradation
/// fraction accounting for test variability.
pub fn compute_rmicro(test: &BiodegradationTest, params: BiodegradationMapParams) -> f32 {
    let b_mean = test.b_mean_percent.clamp(0.0, 100.0);
    let b_std = test.b_std_percent.max(0.0);

    let sigma = b_std.clamp(0.0, params.b_sigma_max);
    let b_eff = (b_mean - sigma).clamp(0.0, 100.0);

    let b_safe_high = params.b_safe_high;
    let b_hard_low = params.b_hard_low;

    let g = if b_eff <= b_hard_low {
        0.0_f32
    } else if b_eff >= b_safe_high {
        1.0_f32
    } else {
        (b_eff - b_hard_low) / (b_safe_high - b_hard_low)
    };

    let rmicro_raw = 1.0_f32 - g;
    rmicro_raw.clamp(0.0, 1.0)
}

/// QSAR-based prior for r_micro: maps a molecular persistence score
/// to a risk coordinate in [0, 1].
pub fn qsar_rmicro_prior(persistence_score: f32) -> f32 {
    persistence_score.clamp(0.0, 1.0)
}

/// Combine test-based r_micro and QSAR prior into a corridor classification.
///
/// Returns the combined coordinate and the corridor label.
pub fn classify_rmicro(rmicro_test: f32, rmicro_qsar: f32) -> (f32, &'static str) {
    let rmicro_combined = 0.5_f32 * rmicro_test + 0.5_f32 * rmicro_qsar;
    let corridor = if rmicro_combined <= 0.2 {
        "SAFE"
    } else if rmicro_combined <= 0.6 {
        "GOLD"
    } else {
        "HARD"
    };
    (rmicro_combined, corridor)
}

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
/// All weights must be non-negative.
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
    /// Knowledge factor in [0, 1].
    pub k: f32,
    /// Eco-impact factor in [0, 1].
    pub e: f32,
    /// Risk factor in [0, 1].
    pub r: f32,
    /// Lyapunov residual V_t.
    pub vt: f32,
}

/// Compute Lyapunov residual V_t = sum_j w_j * r_j^2.
///
/// Inputs are purely numeric; the caller is responsible for normalization and corridor mapping.
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

    #[test]
    fn rmicro_respects_bounds() {
        let params = BiodegradationMapParams {
            b_safe_high: 70.0,
            b_hard_low: 20.0,
            b_sigma_max: 10.0,
        };
        let test = BiodegradationTest {
            b_mean_percent: 80.0,
            b_std_percent: 5.0,
            th_days: 28.0,
        };
        let rmicro = compute_rmicro(&test, params);
        assert!(rmicro >= 0.0 && rmicro <= 1.0);
    }

    #[test]
    fn rmicro_qsar_combination_corridor_labels() {
        let (r_safe, c_safe) = classify_rmicro(0.1, 0.1);
        assert!(r_safe <= 0.2);
        assert_eq!(c_safe, "SAFE");

        let (r_gold, c_gold) = classify_rmicro(0.4, 0.4);
        assert!(r_gold > 0.2 && r_gold <= 0.6);
        assert_eq!(c_gold, "GOLD");

        let (r_hard, c_hard) = classify_rmicro(0.8, 0.8);
        assert!(r_hard > 0.6);
        assert_eq!(c_hard, "HARD");
    }
}
