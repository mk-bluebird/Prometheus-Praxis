// Filename: crates/cyboquatic-ecosafety/src/integrity.rs
use serde::{Deserialize, Serialize};

use crate::{Frame, FrameContext, FrameError, LyapunovResidual, RiskCoord, RiskVector};

/// Integrity diagnostics produced by `IntegrityCheckFrame`.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct IntegrityDiagnostics {
    /// True if any coordinate was NaN or infinite.
    pub has_invalid: bool,
    /// True if any coordinate exceeded the 6σ threshold.
    pub has_outlier: bool,
}

/// Integrity check frame that validates risk vectors before ecosafety evaluation.
///
/// This frame is non-actuating and only flags anomalies:
/// - NaN or infinite risk coordinates.
/// - Coordinates beyond 6σ from their running mean.[file:25]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct IntegrityCheckFrame {
    /// Running mean per coordinate.
    mean: [f64; 7],
    /// Running M2 (sum of squared deviations) per coordinate.
    m2: [f64; 7],
    /// Sample count.
    n: u64,
}

impl IntegrityCheckFrame {
    /// Create a new integrity check frame.
    pub fn new() -> Self {
        Self {
            mean: [0.0; 7],
            m2: [0.0; 7],
            n: 0,
        }
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

    fn update_stats(&mut self, x: [f64; 7]) {
        self.n += 1;
        let n = self.n as f64;
        for i in 0..7 {
            let delta = x[i] - self.mean[i];
            self.mean[i] += delta / n;
            let delta2 = x[i] - self.mean[i];
            self.m2[i] += delta * delta2;
        }
    }

    fn stddevs(&self) -> [f64; 7] {
        let mut out = [0.0; 7];
        if self.n < 2 {
            return out;
        }
        let denom = (self.n - 1) as f64;
        for i in 0..7 {
            let v = (self.m2[i] / denom).max(0.0);
            out[i] = v.sqrt();
        }
        out
    }
}

impl Frame<(), IntegrityDiagnostics> for IntegrityCheckFrame {
    fn run(
        &self,
        ctx: &FrameContext,
        _input: (),
    ) -> Result<(RiskVector, LyapunovResidual, IntegrityDiagnostics), FrameError> {
        let mut frame = self.clone();
        let x = Self::features_from_risk(&ctx.risk_in);

        // NaN / Inf check (non-actuating guard).[file:25]
        let mut has_invalid = false;
        for v in &x {
            if !v.is_finite() {
                has_invalid = true;
                break;
            }
        }

        // Update stats and compute 6σ outliers with numerically stable accumulators.[file:25]
        frame.update_stats(x);
        let std = frame.stddevs();
        let mut has_outlier = false;
        if frame.n > 2 {
            for i in 0..7 {
                let sigma = std[i];
                if sigma > 0.0 {
                    let z = (x[i] - frame.mean[i]) / sigma;
                    if z.abs() > 6.0 {
                        has_outlier = true;
                        break;
                    }
                }
            }
        }

        let diag = IntegrityDiagnostics {
            has_invalid,
            has_outlier,
        };

        if has_invalid {
            return Err(FrameError::InvalidInput(
                "RiskVector contains NaN or infinite values".to_string(),
            ));
        }

        Ok((ctx.risk_in, ctx.residual_in, diag))
    }
}
