#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EvSignalIntegritySummary {
    pub residual_mean: f64,
    pub residual_std: f64,
    pub roh_noise_band: f64,
    pub telemetry_gap_fraction: f64,
}

impl EvSignalIntegritySummary {
    pub fn clamped(self) -> Self {
        Self {
            residual_mean: self.residual_mean,
            residual_std: self.residual_std.max(0.0),
            roh_noise_band: self.roh_noise_band.clamp(0.0, 0.30),
            telemetry_gap_fraction: self.telemetry_gap_fraction.clamp(0.0, 1.0),
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ConformalConfig {
    pub alpha: f64,
    pub aln_floor: f64,
}

pub fn conformal_lower_bound(
    ev: &EvSignalIntegritySummary,
    calib_scores: &[f64],
    cfg: &ConformalConfig,
) -> f64 {
    let ev = ev.clamped();
    let alpha = cfg.alpha.clamp(1e-6, 1.0 - 1e-6);

    if calib_scores.is_empty() {
        return cfg.aln_floor;
    }

    let mut scores = calib_scores.to_vec();
    scores.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));

    let n = scores.len();
    let idx = ((n as f64) * (1.0 - alpha)).ceil() as usize - 1;
    let idx = idx.clamp(0, n - 1);
    let q = scores[idx];

    cfg.aln_floor + ev.residual_mean - q
}

pub fn apply_preemptive_brake(lower_bound: f64, aln_floor: f64) -> bool {
    lower_bound < aln_floor
}
