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

/// Clamped risk coordinate in [0.0, 1.0] using f64 for core ecosafety math.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct RiskCoord {
    /// Normalized risk value in [0.0, 1.0].
    pub value: f64,
}

impl RiskCoord {
    /// Construct a new clamped coordinate.
    pub fn new(raw: f64) -> Self {
        let v = if raw < 0.0 {
            0.0
        } else if raw > 1.0 {
            1.0
        } else {
            raw
        };
        Self { value: v }
    }
}

/// Full ecosafety risk vector using f64 coordinates.
///
/// This matches the ecosafety spine used by other crates.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct RiskVectorF64 {
    pub renergy: RiskCoord,
    pub rhydraulics: RiskCoord,
    pub rbiology: RiskCoord,
    pub rcarbon: RiskCoord,
    pub rmaterials: RiskCoord,
    pub rbiodiversity: RiskCoord,
    pub rsigma: RiskCoord,
}

/// Lyapunov weights for the f64 risk vector.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct LyapunovWeights {
    pub w_energy: f64,
    pub w_hydraulics: f64,
    pub w_biology: f64,
    pub w_carbon: f64,
    pub w_materials: f64,
    pub w_biodiversity: f64,
    pub w_sigma: f64,
}

/// Residual value and max coordinate for Lyapunov analysis.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct Residual {
    /// Lyapunov residual V_t.
    pub vt: f64,
    /// Maximum plane coordinate across the risk vector.
    pub maxcoord: f64,
}

impl LyapunovWeights {
    /// Evaluate residual and max coordinate for a given risk vector.
    pub fn evaluate(&self, rv: &RiskVectorF64) -> Residual {
        let e = rv.renergy.value;
        let h = rv.rhydraulics.value;
        let b = rv.rbiology.value;
        let c = rv.rcarbon.value;
        let m = rv.rmaterials.value;
        let bd = rv.rbiodiversity.value;
        let s = rv.rsigma.value;

        let vt = self.w_energy * e * e
            + self.w_hydraulics * h * h
            + self.w_biology * b * b
            + self.w_carbon * c * c
            + self.w_materials * m * m
            + self.w_biodiversity * bd * bd
            + self.w_sigma * s * s;

        let maxcoord = e.max(h).max(b).max(c).max(m).max(bd).max(s);

        Residual { vt, maxcoord }
    }
}

/// KER window for residual series using f64 residuals.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct KerWindow {
    /// Fraction of nonincreasing residual steps.
    pub k: f64,
    /// Mean eco-benefit across the window.
    pub e: f64,
    /// Maximum risk coordinate across the window.
    pub r: f64,
}

impl KerWindow {
    /// Build a KER window from residual and max risk series.
    pub fn from_residual_series(vts: &[f64], maxrisks: &[f64]) -> Option<Self> {
        if vts.len() < 2 || vts.len() != maxrisks.len() {
            return None;
        }

        let mut nonincreasing_steps = 0usize;
        let mut eco_benefit_sum = 0.0;
        let mut max_risk = 0.0;

        for i in 1..vts.len() {
            if vts[i] <= vts[i - 1] {
                nonincreasing_steps += 1;
            }
        }

        for r in maxrisks {
            if *r > max_risk {
                max_risk = *r;
            }
            eco_benefit_sum += (1.0 - *r).max(0.0);
        }

        let k = nonincreasing_steps as f64 / (vts.len() - 1) as f64;
        let e = eco_benefit_sum / vts.len() as f64;
        let r = max_risk;

        Some(KerWindow { k, e, r })
    }

    /// Check if this window is deployable under a production band.
    pub fn is_deployable(&self, k_min: f64, e_min: f64, r_max: f64) -> bool {
        self.k >= k_min && self.e >= e_min && self.r <= r_max
    }
}

/// Safe-step decision for Lyapunov residual and corridor checks.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum SafeStepDecision {
    /// Step is accepted.
    Accept,
    /// Step is rejected.
    Reject,
}

/// Gate that enforces safestep invariant and corridor bounds.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct SafeStepGate {
    weights: LyapunovWeights,
    vt_current: Residual,
}

impl SafeStepGate {
    /// Construct a new gate from weights and current residual.
    pub fn new(weights: LyapunovWeights, vt_current: Residual) -> Self {
        Self { weights, vt_current }
    }

    /// Evaluate a proposed next risk vector.
    ///
    /// Returns the next residual and the accept/reject decision.
    pub fn evaluate_next(&self, next: RiskVectorF64) -> (Residual, SafeStepDecision) {
        let res_next = self.weights.evaluate(&next);
        let decision = if res_next.vt <= self.vt_current.vt && res_next.maxcoord <= 1.0 {
            SafeStepDecision::Accept
        } else {
            SafeStepDecision::Reject
        };
        (res_next, decision)
    }
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

/// Risk vector with per-plane normalized risk coordinates using f32 for
/// lighter-weight KER scoring and AI-chat tooling.
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

impl RiskVector {
    /// Convert to the core f64-based RiskVectorF64 used by LyapunovWeights.
    pub fn to_f64(&self, rsigma: f32) -> RiskVectorF64 {
        RiskVectorF64 {
            renergy: RiskCoord::new(self.renergy as f64),
            rhydraulics: RiskCoord::new(self.rhydraulic as f64),
            rbiology: RiskCoord::new(self.rbiology as f64),
            rcarbon: RiskCoord::new(self.rcarbon as f64),
            rmaterials: RiskCoord::new(self.rmaterials as f64),
            rbiodiversity: RiskCoord::new(self.rbiodiversity as f64),
            rsigma: RiskCoord::new(rsigma as f64),
        }
    }
}

/// Plane weights used in residual computation for the f32 risk vector.
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

/// Snapshot of KER values and residual for a shard window, using f32.
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

/// Compute Lyapunov residual V_t = sum_j w_j * r_j^2 for f32 vectors.
///
/// Inputs are purely numeric; the caller is responsible for normalization
/// and corridor mapping.
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

#[cfg(kani)]
mod kaniharness {
    use super::*;
    use kani::any;

    #[kani::proof]
    fn residual_non_negative_and_clamped() {
        let re = any::<f64>();
        let rh = any::<f64>();
        let rb = any::<f64>();
        let rc = any::<f64>();
        let rm = any::<f64>();
        let rbd = any::<f64>();
        let rs = any::<f64>();

        let rv = RiskVectorF64 {
            renergy: RiskCoord::new(re),
            rhydraulics: RiskCoord::new(rh),
            rbiology: RiskCoord::new(rb),
            rcarbon: RiskCoord::new(rc),
            rmaterials: RiskCoord::new(rm),
            rbiodiversity: RiskCoord::new(rbd),
            rsigma: RiskCoord::new(rs),
        };

        let w = LyapunovWeights {
            w_energy: 1.0,
            w_hydraulics: 1.0,
            w_biology: 1.0,
            w_carbon: 1.0,
            w_materials: 1.0,
            w_biodiversity: 1.0,
            w_sigma: 1.0,
        };

        let res = w.evaluate(&rv);
        kani::assert!(res.vt >= 0.0);
        kani::assert!(res.maxcoord >= 0.0 && res.maxcoord <= 1.0);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn residual_non_negative_f32() {
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

    #[test]
    fn kerwindow_is_deployable_band_check() {
        let vts = vec![1.0_f64, 0.9, 0.8, 0.75];
        let maxrisks = vec![0.3_f64, 0.25, 0.2, 0.2];
        let kw = KerWindow::from_residual_series(&vts, &maxrisks).expect("window");
        assert!(kw.is_deployable(0.5, 0.5, 0.4));
    }

    #[test]
    fn riskvector_to_f64_conversion() {
        let rv = RiskVector {
            renergy: 0.2,
            rhydraulic: 0.3,
            rbiology: 0.4,
            rcarbon: 0.1,
            rmaterials: 0.5,
            rbiodiversity: 0.6,
            rdataquality: 0.7,
            rtopology: 0.8,
        };
        let core = rv.to_f64(0.9);
        assert!((core.renergy.value - 0.2).abs() < 1e-9);
        assert!((core.rsigma.value - 0.9).abs() < 1e-9);
        assert!(core.maxcoord <= 1.0);
    }
}
