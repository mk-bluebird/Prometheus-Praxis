#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WindowMetrics {
    pub window_id: String,
    pub releases_included: Vec<String>,
    pub lyapunov_stability_improvement: f32,
    pub ker_fairness_improvement: f32,
    pub violation_latency_improvement: f32,
    pub always_improve_score: f32,
}

pub fn compute_always_improve_score(
    lyapunov_stability_improvement: f32,
    ker_fairness_improvement: f32,
    violation_latency_improvement: f32,
) -> f32 {
    const W_L: f32 = 0.4;
    const W_K: f32 = 0.3;
    const W_V: f32 = 0.3;

    W_L * lyapunov_stability_improvement
        + W_K * ker_fairness_improvement
        + W_V * violation_latency_improvement
}

pub fn windows_satisfy_monotonicity(windows: &[WindowMetrics]) -> bool {
    for pair in windows.windows(2) {
        let prev = &pair[0];
        let cur = &pair[1];
        if cur.always_improve_score < prev.always_improve_score {
            return false;
        }
    }
    true
}

pub fn last_window_satisfies_minimum_trend(windows: &[WindowMetrics]) -> bool {
    if let Some(last) = windows.last() {
        last.always_improve_score >= 0.0
    } else {
        false
    }
}
