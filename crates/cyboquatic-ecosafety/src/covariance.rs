// Filename: crates/cyboquatic-ecosafety/src/covariance.rs
#![allow(missing_docs)] // This module is re-exported by lib.rs which denies missing_docs;
// keep internal helpers relaxed but fully document public APIs.

use serde::{Deserialize, Serialize};

use crate::{LyapunovResidual, LyapunovWeights, RiskCoord, RiskVector};

/// Numerically stable scalar accumulator using pairwise summation.
///
/// This implements a simple Kahan-style compensated sum, sufficient
/// for the small vector sizes used in ecosafety kernels.[file:25]
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub struct StableSum {
    sum: f64,
    c: f64,
}

impl StableSum {
    /// Create an empty accumulator.
    pub fn new() -> Self {
        Self { sum: 0.0, c: 0.0 }
    }

    /// Add a value to the accumulator.
    pub fn add(&mut self, x: f64) {
        let y = x - self.c;
        let t = self.sum + y;
        self.c = (t - self.sum) - y;
        self.sum = t;
    }

    /// Final scalar value.
    pub fn value(&self) -> f64 {
        self.sum
    }
}

/// Ecosafety covariance frame configuration.
///
/// When `use_full_cov` is false, only a diagonal variance path is used.
/// When true (requires `full-cov` feature), a full 7×7 covariance is
/// maintained with condition-number regularisation.[file:23][file:25]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcosafetyCovarianceConfig {
    pub use_full_cov: bool,
    /// Minimum allowed eigenvalue after regularisation.
    pub min_eig: f64,
    /// Maximum allowed condition number before regularisation.
    pub max_cond: f64,
}

/// Lyapunov-aware Mahalanobis distance result.
///
/// The distance is computed in a 7D space of normalized risk planes,
/// with Lyapunov weights folded into the metric so that distance
/// remains consistent with the Vt residual ordering.[file:24]
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct LyapunovDistance {
    /// Squared Mahalanobis distance \(d^2\).
    pub d2: f64,
    /// Linear distance \(d\).
    pub d: f64,
}

/// Ecosafety covariance frame state for a 7D risk vector.
///
/// The underlying coordinates are derived from `RiskVector` with a
/// fixed ordering to stay compatible with Lyapunov weights and KER:
/// `[r_cec, r_sat, r_surcharge, r_biodiv, r_vt, r_governance, r_spare]`.[file:23]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcosafetyCovarianceFrameState {
    /// Running mean of each coordinate.
    mean: [f64; 7],
    /// Running count of samples.
    n: u64,
    /// Diagonal variances.
    var: [f64; 7],
    /// Full covariance matrix (row-major 7×7), only used when `full-cov` is enabled.
    #[cfg(feature = "full-cov")]
    cov: [[f64; 7]; 7],
}

/// Input sample for the covariance frame: current risk vector and
/// Vt residual over a rolling window.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CovarianceSample {
    /// Current risk vector.
    pub risk: RiskVector,
    /// Current Lyapunov residual.
    pub residual: LyapunovResidual,
}

/// Output diagnostics from the covariance frame.
///
/// This includes updated risk vector, residual, and optional
/// Lyapunov-weighted distance metrics.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CovarianceOutput {
    /// Updated risk vector (identity in this frame).
    pub risk: RiskVector,
    /// Updated residual (identity in this frame).
    pub residual: LyapunovResidual,
    /// Optional Lyapunov-weighted distance.
    pub distance: Option<LyapunovDistance>,
}

/// Ecosafety covariance frame that can compute Lyapunov-weighted
/// Mahalanobis distances and maintain stable variances/covariances.
///
/// This type is non-actuating and intended strictly for diagnostics
/// and ecosafety evaluation.[file:23][file:24]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcosafetyCovarianceFrame {
    cfg: EcosafetyCovarianceConfig,
    state: EcosafetyCovarianceFrameState,
}

impl EcosafetyCovarianceFrame {
    /// Create a new covariance frame with the given configuration.
    pub fn new(cfg: EcosafetyCovarianceConfig) -> Self {
        let state = EcosafetyCovarianceFrameState {
            mean: [0.0; 7],
            n: 0,
            var: [0.0; 7],
            #[cfg(feature = "full-cov")]
            cov: [[0.0; 7]; 7],
        };
        Self { cfg, state }
    }

    /// Extract the 7D feature vector from a `RiskVector`.
    fn features_from_risk(rv: &RiskVector) -> [f64; 7] {
        [
            rv.r_cec.value(),
            rv.r_sat.value(),
            rv.r_surcharge.value(),
            rv.r_biodiv.value(),
            rv.r_vt.value(),
            rv.r_governance.value(),
            0.0, // spare / future plane.
        ]
    }

    /// Update running mean and variance using numerically stable formulas.
    fn update_diag(&mut self, x: [f64; 7]) {
        self.state.n += 1;
        let n = self.state.n as f64;

        for i in 0..7 {
            let delta = x[i] - self.state.mean[i];
            self.state.mean[i] += delta / n;
            let delta2 = x[i] - self.state.mean[i];
            self.state.var[i] += delta * delta2;
        }
    }

    #[cfg(feature = "full-cov")]
    fn update_full_cov(&mut self, x: [f64; 7]) {
        if self.state.n == 0 {
            return;
        }
        let n = self.state.n as f64;
        let mut centered = [0.0; 7];
        for i in 0..7 {
            centered[i] = x[i] - self.state.mean[i];
        }
        for i in 0..7 {
            for j in 0..7 {
                // Incremental covariance update (Welford-style).
                self.state.cov[i][j] += (centered[i] * centered[j] - self.state.cov[i][j]) / n;
            }
        }
    }

    /// Compute per-coordinate standard deviations.
    fn stddevs(&self) -> [f64; 7] {
        let mut out = [0.0; 7];
        if self.state.n < 2 {
            return out;
        }
        let denom = (self.state.n - 1) as f64;
        for i in 0..7 {
            let v = (self.state.var[i] / denom).max(0.0);
            out[i] = v.sqrt();
        }
        out
    }

    /// Compute Lyapunov-weighted Mahalanobis distance using diagonal covariance.
    ///
    /// This is always available and uses numerically stable accumulators.
    fn distance_diag(
        &self,
        x: [f64; 7],
        weights: &LyapunovWeights,
    ) -> LyapunovDistance {
        let std = self.stddevs();
        let w = [
            weights.w_cec,
            weights.w_sat,
            weights.w_surcharge,
            weights.w_biodiv,
            weights.w_vt,
            weights.w_governance,
            1.0,
        ];

        let mut acc = StableSum::new();
        for i in 0..7 {
            let sigma = std[i].max(1e-9);
            let zi = (x[i] - self.state.mean[i]) / sigma;
            // Fold Lyapunov weight into metric to keep ordering consistent with Vt.[file:24]
            let term = w[i].max(0.0) * zi * zi;
            acc.add(term);
        }
        let d2 = acc.value().max(0.0);
        let d = d2.sqrt();
        LyapunovDistance { d2, d }
    }

    #[cfg(feature = "full-cov")]
    fn regularise_cov(&mut self, ker: (f64, f64, f64)) {
        // Tune diagonal regularisation λ(K,E,R) = λ0 * (1 - K) + λ1 * R, bounded.[file:25]
        let (k, e, r) = ker;
        let base = 1e-6;
        let lambda = (base * (1.0 - k).max(0.0) + base * 10.0 * r.max(0.0)).max(base);
        for i in 0..7 {
            self.state.cov[i][i] += lambda;
        }
    }

    #[cfg(feature = "full-cov")]
    fn condition_number(&self) -> f64 {
        // For small 7×7 SPD-ish matrices, approximate condition number via
        // power iterations on cov and cov^{-1} is overkill; instead, use
        // Gershgorin-style row sums as cheap bounds.[file:23]
        let mut min_eig = f64::MAX;
        let mut max_eig = 0.0;
        for i in 0..7 {
            let mut rowsum = 0.0;
            let mut diag = self.state.cov[i][i].abs();
            for j in 0..7 {
                if i == j {
                    continue;
                }
                rowsum += self.state.cov[i][j].abs();
            }
            let low = (diag - rowsum).max(0.0);
            let high = diag + rowsum;
            if low < min_eig {
                min_eig = low;
            }
            if high > max_eig {
                max_eig = high;
            }
        }
        if min_eig <= 0.0 {
            return f64::INFINITY;
        }
        max_eig / min_eig
    }

    #[cfg(feature = "full-cov")]
    fn distance_full(
        &mut self,
        x: [f64; 7],
        weights: &LyapunovWeights,
        ker: (f64, f64, f64),
    ) -> LyapunovDistance {
        // Regularise if condition number too large.
        if self.condition_number() > self.cfg.max_cond {
            self.regularise_cov(ker);
        }
        let mut z = [0.0; 7];
        for i in 0..7 {
            z[i] = x[i] - self.state.mean[i];
        }

        // Solve C^{-1} z via simple Gaussian elimination (7×7 is small).
        let mut a = self.state.cov;
        let mut y = z;

        for i in 0..7 {
            let pivot = a[i][i];
            let piv = if pivot.abs() < 1e-12 { 1e-12 } else { pivot };
            for j in 0..7 {
                a[i][j] /= piv;
            }
            y[i] /= piv;
            for k in 0..7 {
                if k == i {
                    continue;
                }
                let factor = a[k][i];
                for j in 0..7 {
                    a[k][j] -= factor * a[i][j];
                }
                y[k] -= factor * y[i];
            }
        }

        // Lyapunov-weighted Mahalanobis d^2 = z^T C^{-1} z with weights.[file:24]
        let w = [
            weights.w_cec,
            weights.w_sat,
            weights.w_surcharge,
            weights.w_biodiv,
            weights.w_vt,
            weights.w_governance,
            1.0,
        ];
        let mut acc = StableSum::new();
        for i in 0..7 {
            let term = w[i].max(0.0) * z[i] * y[i];
            acc.add(term);
        }
        let d2 = acc.value().max(0.0);
        let d = d2.sqrt();
        LyapunovDistance { d2, d }
    }

    /// Run a covariance update and (optionally) compute a Lyapunov-weighted distance.
    ///
    /// This method is non-actuating and intended to be called from a higher-level
    /// `Frame` implementation that wraps it into the generic ecosafety pipeline.
    pub fn run_step(
        &mut self,
        sample: CovarianceSample,
        weights: &LyapunovWeights,
        ker_window: (f64, f64, f64),
        compute_distance: bool,
    ) -> CovarianceOutput {
        let x = Self::features_from_risk(&sample.risk);

        // Update numerically stable diagonal statistics.
        self.update_diag(x);
        #[cfg(feature = "full-cov")]
        if self.cfg.use_full_cov {
            self.update_full_cov(x);
        }

        let distance = if compute_distance {
            #[cfg(feature = "full-cov")]
            {
                if self.cfg.use_full_cov {
                    Some(self.distance_full(x, weights, ker_window))
                } else {
                    Some(self.distance_diag(x, weights))
                }
            }
            #[cfg(not(feature = "full-cov"))]
            {
                Some(self.distance_diag(x, weights))
            }
        } else {
            None
        };

        CovarianceOutput {
            risk: sample.risk,
            residual: sample.residual,
            distance,
        }
    }
}
