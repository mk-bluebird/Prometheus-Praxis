#![allow(dead_code)]
#![forbid(unsafe_code)]

use always_improve_core::{
    WindowMetrics, compute_always_improve_score, last_window_satisfies_minimum_trend,
    windows_satisfy_monotonicity,
};

/// Harness 1: compute_always_improve_score stays within [-1,1] when inputs are in [-1,1].
#[kani::proof]
fn kani_always_improve_score_range() {
    let l: f32 = kani::any();
    let k: f32 = kani::any();
    let v: f32 = kani::any();

    kani::assume(l >= -1.0 && l <= 1.0);
    kani::assume(k >= -1.0 && k <= 1.0);
    kani::assume(v >= -1.0 && v <= 1.0);

    let score = compute_always_improve_score(l, k, v);
    // Max absolute value when all components are +/-1.0.
    assert!(score >= -1.0 && score <= 1.0);
}

/// Harness 2: windows_satisfy_monotonicity returns false when a later window
/// has a lower score than an earlier one.
#[kani::proof]
fn kani_always_improve_monotonicity_detection() {
    let w1 = WindowMetrics {
        window_id: "w1".to_string(),
        releases_included: vec!["r1".to_string()],
        lyapunov_stability_improvement: 0.1,
        ker_fairness_improvement: 0.0,
        violation_latency_improvement: 0.0,
        always_improve_score: 0.1,
    };

    let w2 = WindowMetrics {
        window_id: "w2".to_string(),
        releases_included: vec!["r2".to_string()],
        lyapunov_stability_improvement: -0.5,
        ker_fairness_improvement: 0.0,
        violation_latency_improvement: 0.0,
        always_improve_score: -0.5,
    };

    let windows = vec![w1.clone(), w2.clone()];
    assert!(!windows_satisfy_monotonicity(&windows));

    let windows_ok = vec![w1, WindowMetrics { always_improve_score: 0.2, ..w2 }];
    assert!(windows_satisfy_monotonicity(&windows_ok));
}

/// Harness 3: last_window_satisfies_minimum_trend enforces score >= 0.0.
#[kani::proof]
fn kani_always_improve_minimum_trend() {
    let w_neg = WindowMetrics {
        window_id: "w".to_string(),
        releases_included: vec!["r".to_string()],
        lyapunov_stability_improvement: -1.0,
        ker_fairness_improvement: -1.0,
        violation_latency_improvement: -1.0,
        always_improve_score: -0.5,
    };
    assert!(!last_window_satisfies_minimum_trend(&[w_neg.clone()]));

    let w_pos = WindowMetrics {
        always_improve_score: 0.1,
        ..w_neg
    };
    assert!(last_window_satisfies_minimum_trend(&[w_pos]));
}
