// Filename: crates/cyboquatic-ecosafety/src/lyapunov_regime.rs
// Lyapunov stability and regime-shift detection over Vt histories.

use serde::{Deserialize, Serialize};

/// Rolling Vt history for a node.[file:23]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct VtHistory {
    capacity: usize,
    vt: std::collections::VecDeque<f64>,
}

impl VtHistory {
    pub fn new(capacity: usize) -> Self {
        assert!(capacity > 1);
        Self {
            capacity,
            vt: std::collections::VecDeque::with_capacity(capacity),
        }
    }

    pub fn push(&mut self, v: f64) {
        if self.vt.len() == self.capacity {
            self.vt.pop_front();
        }
        self.vt.push_back(v);
    }

    pub fn len(&self) -> usize {
        self.vt.len()
    }

    pub fn as_vec(&self) -> Vec<f64> {
        self.vt.iter().copied().collect()
    }
}

/// Lyapunov stability diagnostics for regime-shift detection.[file:23]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LyapunovStabilityDiagnostics {
    pub local_exponent: f64,
    pub changepoint_score: f64,
    pub pre_failure_flag: bool,
}

/// Frame operating purely on Vt histories to detect regime shifts.[file:23]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LyapunovStabilityFrame {
    /// Minimum number of samples required.
    pub min_samples: usize,
    /// Threshold on local exponent above which we consider instability.
    pub exponent_threshold: f64,
    /// Threshold on changepoint score.
    pub changepoint_threshold: f64,
}

impl LyapunovStabilityFrame {
    fn estimate_local_exponent(v: &[f64]) -> f64 {
        if v.len() < 2 {
            return 0.0;
        }
        // Simple least-squares fit of log(Vt) vs time index to approximate exponent.[file:23]
        let mut xs = Vec::with_capacity(v.len());
        let mut ys = Vec::with_capacity(v.len());
        for (i, &val) in v.iter().enumerate() {
            if val <= 0.0 {
                continue;
            }
            xs.push(i as f64);
            ys.push(val.ln());
        }
        let n = xs.len();
        if n < 2 {
            return 0.0;
        }
        let mut sum_x = 0.0;
        let mut sum_y = 0.0;
        let mut sum_xy = 0.0;
        let mut sum_x2 = 0.0;
        for i in 0..n {
            sum_x += xs[i];
            sum_y += ys[i];
            sum_xy += xs[i] * ys[i];
            sum_x2 += xs[i] * xs[i];
        }
        let denom = (n as f64) * sum_x2 - sum_x * sum_x;
        if denom.abs() < 1e-12 {
            return 0.0;
        }
        let slope = ((n as f64) * sum_xy - sum_x * sum_y) / denom;
        slope
    }

    fn changepoint_score(v: &[f64]) -> f64 {
        if v.len() < 4 {
            return 0.0;
        }
        let mid = v.len() / 2;
        let (left, right) = v.split_at(mid);
        let mean = |s: &[f64]| -> f64 {
            if s.is_empty() {
                return 0.0;
            }
            let mut acc = 0.0;
            for &x in s {
                acc += x;
            }
            acc / (s.len() as f64)
        };
        let ml = mean(left);
        let mr = mean(right);
        (mr - ml).abs()
    }

    pub fn analyze(&self, history: &VtHistory) -> LyapunovStabilityDiagnostics {
        let vt = history.as_vec();
        if vt.len() < self.min_samples {
            return LyapunovStabilityDiagnostics {
                local_exponent: 0.0,
                changepoint_score: 0.0,
                pre_failure_flag: false,
            };
        }
        let exp = Self::estimate_local_exponent(&vt);
        let cps = Self::changepoint_score(&vt);
        let pre_failure = exp > self.exponent_threshold || cps > self.changepoint_threshold;
        LyapunovStabilityDiagnostics {
            local_exponent: exp,
            changepoint_score: cps,
            pre_failure_flag: pre_failure,
        }
    }
}
