#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Summary of EV signal integrity metrics used for ALN/conformal safety logic.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EvSignalIntegritySummary {
    /// Mean residual of the predictive model (unbounded; diagnostic only).
    pub residual_mean: f64,
    /// Standard deviation of residuals (clamped to be non-negative).
    pub residual_std: f64,
    /// RoH noise band for telemetry residuals, corridor-bounded in [0.0, 0.30].
    pub roh_noise_band: f64,
    /// Fraction of telemetry gaps in the window, corridor-bounded in [0.0, 1.0].
    pub telemetry_gap_fraction: f64,
}

impl EvSignalIntegritySummary {
    /// Clamp all corridor-governed fields into their allowed ranges.
    pub fn clamped(self) -> Self {
        Self {
            residual_mean: self.residual_mean,
            residual_std: self.residual_std.max(0.0),
            roh_noise_band: self.roh_noise_band.clamp(0.0, 0.30),
            telemetry_gap_fraction: self.telemetry_gap_fraction.clamp(0.0, 1.0),
        }
    }
}

/// Configuration for a non-actuating conformal lower-bound kernel on residuals.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ConformalConfig {
    /// Target miscoverage level, corridor-bounded in (0, 1).
    pub alpha: f64,
    /// ALN floor for normalized state (e.g. 0.0), used as conservative brake floor.
    pub aln_floor: f64,
}

/// Compute a conservative conformal lower bound on EV residuals relative to an ALN floor.
///
/// - `ev`: EV signal integrity summary, corridor-clamped before use.
/// - `calib_scores`: calibration residual scores used for quantile estimation.
/// - `cfg`: conformal configuration with bounded `alpha` and ALN floor.
///
/// When `calib_scores` is empty, this returns `cfg.aln_floor` to force preemptive brake
/// logic upstream in non-actuating governance kernels.
pub fn conformal_lower_bound(
    ev: &EvSignalIntegritySummary,
    calib_scores: &[f64],
    cfg: &ConformalConfig,
) -> f64 {
    let ev = ev.clamped();
    let alpha = cfg.alpha.clamp(1e-6, 1.0 - 1e-6);

    if calib_scores.is_empty() {
        // Conservative: no calibration → return floor to force Brake logic.
        return cfg.aln_floor;
    }

    // Simple quantile-based inductive conformal bound on residuals.
    let mut scores = calib_scores.to_vec();
    scores.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));

    let idx = ((scores.len() as f64) * (1.0 - alpha)).ceil() as usize - 1;
    let idx = idx.clamp(0, scores.len() - 1);
    let q = scores[idx];

    // Lower bound = ALN floor + worst-case residual shift.
    cfg.aln_floor + ev.residual_mean - q
}

/// Non-actuating brake decision: returns `true` when the lower bound falls below the ALN floor.
///
/// This is intended to feed upstream lane/CI logic, never hardware actuation directly.
pub fn apply_preemptive_brake(lower_bound: f64, aln_floor: f64) -> bool {
    lower_bound < aln_floor
}
