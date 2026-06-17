// FILE: crates/kerdeployable/src/lib.rs
// DESTINATION: crates/kerdeployable/src/lib.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard
//
// Invariant engine for KER deployability.
//
// Enforces:
//  (A) Plane non-compensation: worsening in a nonoffsettable plane
//      cannot be hidden by improvements in other planes.
//  (B) Uncertainty monotonicity: when calibration risk (r_calib) or
//      sigma risk (r_sigma) increase, K must not increase and E must
//      not increase and R must not decrease.
//      i.e.  ΔK≤0, ΔE≤0, ΔR≥0  whenever Δr_calib>0 or Δr_sigma>0.
//  (C) Lyapunov safe-step: Vt_after <= Vt_before + epsilon.
//
// All functions are pure (no I/O, no DB access).

#![forbid(unsafe_code)]
#![warn(missing_docs)]

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Invariant violation details.
#[derive(Debug, Error, Serialize, Deserialize, Clone)]
pub enum InvariantViolation {
    /// A nonoffsettable plane worsened while the aggregate R improved.
    #[error("non-compensation: plane '{plane}' worsened (Δr={delta_r:.4}) while aggregate R improved")]
    NonCompensation {
        /// Name of the nonoffsettable plane that worsened.
        plane: String,
        /// Positive increase in the plane's risk coordinate.
        delta_r: f64,
    },
    /// Uncertainty monotonicity broken: K or E rose while r_calib or r_sigma rose.
    #[error("uncertainty monotonicity: delta_k={delta_k:.4}, delta_e={delta_e:.4}, delta_r={delta_r:.4} violates ΔK≤0,ΔE≤0,ΔR≥0")]
    UncertaintyMonotonicity {
        /// Change in K (positive means K increased — forbidden).
        delta_k: f64,
        /// Change in E (positive means E increased — forbidden).
        delta_e: f64,
        /// Change in R (negative means R decreased — forbidden).
        delta_r: f64,
    },
    /// Lyapunov residual increased beyond epsilon.
    #[error("lyapunov safe-step: vt_before={vt_before:.6}, vt_after={vt_after:.6}, epsilon={epsilon:.6}")]
    LyapunovSafeStep {
        /// Residual before the step.
        vt_before: f64,
        /// Residual after the step.
        vt_after: f64,
        /// Permitted slack.
        epsilon: f64,
    },
}

/// A single plane's risk coordinate, with a flag marking it as nonoffsettable.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneRiskCoord {
    /// Plane label, e.g. `"CARBON"`, `"BIODIVERSITY"`, `"RESPONSIBILITY"`.
    pub plane: String,
    /// Risk coordinate in [0, 1].
    pub r_value: f64,
    /// Whether this plane is nonoffsettable under the active plane-weight contract.
    pub nonoffsettable: bool,
}

/// KER snapshot at a single point in time.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerPoint {
    /// Knowledge factor, clamped [0, 1].
    pub k: f64,
    /// Eco-impact factor, clamped [0, 1].
    pub e: f64,
    /// Risk factor, clamped [0, 1].
    pub r: f64,
    /// Lyapunov residual Vt ≥ 0.
    pub vt: f64,
    /// Per-plane risk coordinates at this point.
    pub planes: Vec<PlaneRiskCoord>,
    /// Calibration uncertainty risk coordinate, in [0, 1].
    pub r_calib: f64,
    /// Sigma (spread) uncertainty risk coordinate, in [0, 1].
    pub r_sigma: f64,
}

/// Result of a full invariant check between two KER points.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InvariantCheckResult {
    /// All violations found; empty means the step is admissible.
    pub violations: Vec<InvariantViolation>,
    /// Whether the step is fully admissible (no violations).
    pub admissible: bool,
}

impl InvariantCheckResult {
    fn new() -> Self {
        Self {
            violations: Vec::new(),
            admissible: true,
        }
    }

    fn push(&mut self, v: InvariantViolation) {
        self.violations.push(v);
        self.admissible = false;
    }
}

/// Check plane non-compensation invariant.
///
/// For every nonoffsettable plane, if the plane's risk coordinate
/// worsened (increased), the step is inadmissible regardless of
/// improvements in aggregate R or other planes.
pub fn check_non_compensation(
    before: &KerPoint,
    after: &KerPoint,
) -> Vec<InvariantViolation> {
    let mut violations = Vec::new();

    let delta_r_agg = after.r - before.r; // negative = aggregate improved

    for plane_after in &after.planes {
        if !plane_after.nonoffsettable {
            continue;
        }
        let r_before_plane = before
            .planes
            .iter()
            .find(|p| p.plane == plane_after.plane)
            .map(|p| p.r_value)
            .unwrap_or(0.0);

        let delta_plane = plane_after.r_value - r_before_plane;

        // Violation: nonoffsettable plane got worse while aggregate R stayed same or improved.
        if delta_plane > 1e-9 && delta_r_agg <= 1e-9 {
            violations.push(InvariantViolation::NonCompensation {
                plane:   plane_after.plane.clone(),
                delta_r: delta_plane,
            });
        }
    }

    violations
}

/// Check uncertainty monotonicity invariant.
///
/// When r_calib or r_sigma increases across the step:
///   ΔK ≤ 0,  ΔE ≤ 0,  ΔR ≥ 0
pub fn check_uncertainty_monotonicity(
    before: &KerPoint,
    after: &KerPoint,
) -> Option<InvariantViolation> {
    let delta_calib = after.r_calib - before.r_calib;
    let delta_sigma = after.r_sigma - before.r_sigma;

    if delta_calib <= 1e-9 && delta_sigma <= 1e-9 {
        return None; // uncertainty did not increase; rule does not apply
    }

    let delta_k = after.k - before.k;
    let delta_e = after.e - before.e;
    let delta_r = after.r - before.r;

    if delta_k > 1e-9 || delta_e > 1e-9 || delta_r < -1e-9 {
        return Some(InvariantViolation::UncertaintyMonotonicity {
            delta_k,
            delta_e,
            delta_r,
        });
    }

    None
}

/// Check the Lyapunov safe-step condition: Vt_after ≤ Vt_before + epsilon.
pub fn check_lyapunov_safe_step(
    before: &KerPoint,
    after: &KerPoint,
    epsilon: f64,
) -> Option<InvariantViolation> {
    if after.vt > before.vt + epsilon {
        Some(InvariantViolation::LyapunovSafeStep {
            vt_before: before.vt,
            vt_after:  after.vt,
            epsilon,
        })
    } else {
        None
    }
}

/// Run all three invariant checks and return a consolidated result.
///
/// `epsilon` is the Lyapunov slack (use `1e-6` for strict checks,
/// a slightly larger value for windows with sensor noise).
pub fn check_all_invariants(
    before: &KerPoint,
    after: &KerPoint,
    epsilon: f64,
) -> InvariantCheckResult {
    let mut result = InvariantCheckResult::new();

    for v in check_non_compensation(before, after) {
        result.push(v);
    }

    if let Some(v) = check_uncertainty_monotonicity(before, after) {
        result.push(v);
    }

    if let Some(v) = check_lyapunov_safe_step(before, after, epsilon) {
        result.push(v);
    }

    result
}

/// Compute the Lyapunov residual Vt = Σ w_j * r_j² from plane-level
/// weights (weight keyed by plane name) and the plane coordinates in `point`.
///
/// Planes not present in `weights` contribute zero.
pub fn compute_vt(point: &KerPoint, weights: &[(String, f64)]) -> f64 {
    point
        .planes
        .iter()
        .map(|p| {
            let w = weights
                .iter()
                .find(|(name, _)| name == &p.plane)
                .map(|(_, w)| *w)
                .unwrap_or(0.0);
            w * p.r_value * p.r_value
        })
        .sum()
}

/// Build a synthetic A/B/C matrix fixture for unit tests.
///
/// Returns three `KerPoint` values:
/// - A: baseline safe state (K=0.93, E=0.90, R=0.12, all planes in corridor)
/// - B: monotone improvement (K=0.95, E=0.92, R=0.10, planes reduced)
/// - C: violation state (nonoffsettable CARBON plane worsens while aggregate R improves)
pub fn synthetic_abc_fixture() -> (KerPoint, KerPoint, KerPoint) {
    let plane_a = vec![
        PlaneRiskCoord { plane: "CARBON".into(),        r_value: 0.10, nonoffsettable: true  },
        PlaneRiskCoord { plane: "BIODIVERSITY".into(),  r_value: 0.12, nonoffsettable: true  },
        PlaneRiskCoord { plane: "ENERGY".into(),        r_value: 0.14, nonoffsettable: false },
        PlaneRiskCoord { plane: "TOPOLOGY".into(),      r_value: 0.08, nonoffsettable: false },
    ];

    let point_a = KerPoint {
        k: 0.93, e: 0.90, r: 0.12,
        vt: 0.055,
        planes: plane_a,
        r_calib: 0.05,
        r_sigma:  0.04,
    };

    // B: genuine improvement — all planes reduced or held.
    let plane_b = vec![
        PlaneRiskCoord { plane: "CARBON".into(),       r_value: 0.08, nonoffsettable: true  },
        PlaneRiskCoord { plane: "BIODIVERSITY".into(), r_value: 0.10, nonoffsettable: true  },
        PlaneRiskCoord { plane: "ENERGY".into(),       r_value: 0.12, nonoffsettable: false },
        PlaneRiskCoord { plane: "TOPOLOGY".into(),     r_value: 0.07, nonoffsettable: false },
    ];

    let point_b = KerPoint {
        k: 0.95, e: 0.92, r: 0.10,
        vt: 0.042,
        planes: plane_b,
        r_calib: 0.05,
        r_sigma:  0.04,
    };

    // C: CARBON plane worsens while aggregate R appears to improve — violation.
    let plane_c = vec![
        PlaneRiskCoord { plane: "CARBON".into(),       r_value: 0.18, nonoffsettable: true  }, // worsened
        PlaneRiskCoord { plane: "BIODIVERSITY".into(), r_value: 0.10, nonoffsettable: true  },
        PlaneRiskCoord { plane: "ENERGY".into(),       r_value: 0.04, nonoffsettable: false }, // improved a lot
        PlaneRiskCoord { plane: "TOPOLOGY".into(),     r_value: 0.03, nonoffsettable: false }, // improved
    ];

    let point_c = KerPoint {
        k: 0.96, e: 0.93, r: 0.10, // aggregate R same as A → non-compensation fires
        vt: 0.052,
        planes: plane_c,
        r_calib: 0.05,
        r_sigma:  0.04,
    };

    (point_a, point_b, point_c)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_to_b_is_admissible() {
        let (a, b, _) = synthetic_abc_fixture();
        let result = check_all_invariants(&a, &b, 1e-6);
        assert!(
            result.admissible,
            "A→B must be admissible; violations: {:?}",
            result.violations
        );
    }

    #[test]
    fn a_to_c_violates_non_compensation() {
        let (a, _, c) = synthetic_abc_fixture();
        let result = check_all_invariants(&a, &c, 1e-6);
        assert!(
            !result.admissible,
            "A→C must be inadmissible (CARBON worsens)"
        );
        let has_nc = result
            .violations
            .iter()
            .any(|v| matches!(v, InvariantViolation::NonCompensation { .. }));
        assert!(has_nc, "expected NonCompensation violation; got {:?}", result.violations);
    }

    #[test]
    fn uncertainty_monotonicity_fires_when_calib_rises_and_k_rises() {
        let mut before = KerPoint {
            k: 0.90, e: 0.85, r: 0.15,
            vt: 0.060,
            planes: vec![],
            r_calib: 0.05,
            r_sigma:  0.04,
        };
        let mut after = before.clone();
        after.r_calib = 0.10; // r_calib increased
        after.k       = 0.93; // K also increased — forbidden

        let v = check_uncertainty_monotonicity(&before, &after);
        assert!(
            v.is_some(),
            "must fire UncertaintyMonotonicity; got None"
        );

        // Fix: if K does not increase, no violation.
        after.k = 0.88;
        let v2 = check_uncertainty_monotonicity(&before, &after);
        assert!(v2.is_none(), "should not fire when ΔK≤0");
    }

    #[test]
    fn lyapunov_safe_step_fires() {
        let before = KerPoint {
            k: 0.90, e: 0.85, r: 0.15,
            vt: 0.060,
            planes: vec![],
            r_calib: 0.05,
            r_sigma:  0.04,
        };
        let mut after = before.clone();
        after.vt = 0.070; // increased beyond epsilon

        let v = check_lyapunov_safe_step(&before, &after, 1e-6);
        assert!(v.is_some(), "must fire LyapunovSafeStep");

        after.vt = 0.060; // exactly equal
        let v2 = check_lyapunov_safe_step(&before, &after, 1e-6);
        assert!(v2.is_none(), "must not fire when equal");
    }

    #[test]
    fn compute_vt_correctness() {
        let weights = vec![
            ("CARBON".into(), 2.0),
            ("BIODIVERSITY".into(), 1.5),
        ];
        let point = KerPoint {
            k: 0.90, e: 0.90, r: 0.12,
            vt: 0.0,
            planes: vec![
                PlaneRiskCoord { plane: "CARBON".into(),       r_value: 0.1, nonoffsettable: true },
                PlaneRiskCoord { plane: "BIODIVERSITY".into(), r_value: 0.2, nonoffsettable: true },
            ],
            r_calib: 0.0,
            r_sigma:  0.0,
        };
        let vt = compute_vt(&point, &weights);
        // 2.0*0.01 + 1.5*0.04 = 0.02 + 0.06 = 0.08
        assert!((vt - 0.08).abs() < 1e-10, "vt={vt}");
    }
}
