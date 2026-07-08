// filename: crates/cyboquatic-ecosafety/src/covariance.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]
#![allow(missing_docs)]

use serde::{Deserialize, Serialize};

use crate::error::{FrameError, invariant_error};
use crate::{LyapunovResidual, LyapunovWeights, RiskCoord, RiskVector};
use crate::shard::{KerSnapshot, ShardUpdate, Lane};

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub struct StableSum {
    sum: f64,
    c: f64,
}

impl StableSum {
    pub fn new() -> Self {
        Self { sum: 0.0, c: 0.0 }
    }

    pub fn add(&mut self, x: f64) {
        let y = x - self.c;
        let t = self.sum + y;
        self.c = (t - self.sum) - y;
        self.sum = t;
    }

    pub fn value(&self) -> f64 {
        self.sum
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcosafetyCovarianceConfig {
    pub use_full_cov: bool,
    pub min_eig: f64,
    pub max_cond: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct LyapunovDistance {
    pub d2: f64,
    pub d: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcosafetyCovarianceFrameState {
    mean: [f64; 7],
    n: u64,
    var: [f64; 7],
    #[cfg(feature = "full-cov")]
    cov: [[f64; 7]; 7],
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CovarianceSample {
    pub risk: RiskVector,
    pub residual: LyapunovResidual,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CovarianceOutput {
    pub risk: RiskVector,
    pub residual: LyapunovResidual,
    pub distance: Option<LyapunovDistance>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcosafetyCovarianceFrame {
    cfg: EcosafetyCovarianceConfig,
    state: EcosafetyCovarianceFrameState,
}

impl EcosafetyCovarianceFrame {
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

    fn features_from_risk(rv: &RiskVector) -> [f64; 7] {
        [
            rv.r_cec.value(),
            rv.r_sat.value(),
            rv.r_surcharge.value(),
            rv.r_biodiv.value(),
            rv.r_vt.value(),
            rv.r_governance.value(),
            0.0,
        ]
    }

    fn update_diag(&mut self, x: [f64; 7]) {
        self.state.n = self.state.n.saturating_add(1);
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
                self.state.cov[i][j] += (centered[i] * centered[j] - self.state.cov[i][j]) / n;
            }
        }
    }

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
            let term = w[i].max(0.0) * zi * zi;
            acc.add(term);
        }
        let d2 = acc.value().max(0.0);
        let d = d2.sqrt();
        LyapunovDistance { d2, d }
    }

    #[cfg(feature = "full-cov")]
    fn regularise_cov(&mut self, ker: (f64, f64, f64)) {
        let (k, _e, r) = ker;
        let base = 1e-6;
        let lambda = (base * (1.0 - k).max(0.0) + base * 10.0 * r.max(0.0)).max(base);
        for i in 0..7 {
            self.state.cov[i][i] += lambda;
        }
    }

    #[cfg(feature = "full-cov")]
    fn condition_number(&self) -> f64 {
        let mut min_eig = f64::MAX;
        let mut max_eig = 0.0;
        for i in 0..7 {
            let mut rowsum = 0.0;
            let diag = self.state.cov[i][i].abs();
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
        if self.condition_number() > self.cfg.max_cond {
            self.regularise_cov(ker);
        }
        let mut z = [0.0; 7];
        for i in 0..7 {
            z[i] = x[i] - self.state.mean[i];
        }

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

    pub fn run_step(
        &mut self,
        sample: CovarianceSample,
        weights: &LyapunovWeights,
        ker_window: (f64, f64, f64),
        compute_distance: bool,
    ) -> CovarianceOutput {
        let x = Self::features_from_risk(&sample.risk);

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

pub fn run_covariance_frame(
    shard_id: &str,
    node_id: &str,
    region_id: &str,
    input_values: &[f32],
    prev_snapshot: Option<KerSnapshot>,
) -> Result<ShardUpdate, FrameError> {
    if input_values.is_empty() {
        return Err(FrameError::MissingField("covariance_input_values"));
    }

    let mut update = ShardUpdate::new(
        shard_id.to_string(),
        node_id.to_string(),
        region_id.to_string(),
    );
    update.ker_snapshot_before = prev_snapshot.clone();

    let n = input_values.len() as f32;
    if n <= 0.0 {
        let err = FrameError::NumericIssue("covariance sample size <= 0");
        update.add_error(err.clone());
        return Err(err);
    }

    let mean = input_values.iter().fold(0.0_f32, |acc, v| acc + *v) / n;
    let mut var_acc = 0.0_f32;
    for v in input_values {
        let diff = *v - mean;
        var_acc += diff * diff;
    }
    let variance = var_acc / n;

    if !variance.is_finite() {
        let err = FrameError::NumericIssue("covariance variance is non-finite");
        update.add_error(err.clone());
        return Err(err);
    }

    if variance < 0.0_f32 {
        let err = invariant_error("covariance variance < 0.0");
        update.add_error(err.clone());
        return Err(err);
    }

    let new_snapshot = KerSnapshot {
        k: prev_snapshot.as_ref().map(|s| s.k).unwrap_or(0.0),
        e: prev_snapshot.as_ref().map(|s| s.e).unwrap_or(0.0),
        r: variance,
        vt: variance,
        lane: prev_snapshot
            .as_ref()
            .map(|s| s.lane)
            .unwrap_or(Lane::Research),
        is_speculative: false,
    };

    update.ker_snapshot_after = Some(new_snapshot);
    Ok(update)
}
