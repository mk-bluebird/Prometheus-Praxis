// File: crates/drainage_decay/tests/kani_ev_conformal.rs

#![allow(dead_code)]
#![forbid(unsafe_code)]

use drainage_decay::{
    ConformalConfig,
    EvSignalIntegritySummary,
    apply_preemptive_brake,
    conformal_lower_bound,
};

/// Helper: baseline EV signal integrity within RoH noise corridors.
fn baseline_ev() -> EvSignalIntegritySummary {
    EvSignalIntegritySummary {
        residual_mean: 0.0,
        residual_std: 0.05,
        roh_noise_band: 0.10,
        telemetry_gap_fraction: 0.05,
    }
}

/// Harness 1: If conformal_lower_bound < aln_floor, Brake must be triggered.
///
/// This is the core ALN invariant encoded in ev-conformal-drainage.v1.aln.
#[kani::proof]
fn kani_ev_conformal_brake_on_floor_crossing() {
    let ev = baseline_ev();

    // Simple calibration scores; Kani can explore any values here.
    let s1: f64 = kani::any();
    let s2: f64 = kani::any();
    let s3: f64 = kani::any();

    let calib_scores = [s1, s2, s3];

    let cfg = ConformalConfig {
        alpha: 0.1,
        aln_floor: 0.0,
    };

    let lb = conformal_lower_bound(&ev, &calib_scores, &cfg);
    let brake = apply_preemptive_brake(lb, cfg.aln_floor);

    if lb < cfg.aln_floor {
        assert!(brake);
    }
}

/// Harness 2: With empty calibration, lower bound equals ALN floor
/// and Brake is not triggered.
///
/// This validates the conservative default behavior.
#[kani::proof]
fn kani_ev_conformal_empty_calibration_floor_default() {
    let ev = baseline_ev();
    let cfg = ConformalConfig {
        alpha: 0.1,
        aln_floor: 0.0,
    };

    let calib_scores: [f64; 0] = [];
    let lb = conformal_lower_bound(&ev, &calib_scores, &cfg);

    assert!(lb == cfg.aln_floor);

    let brake = apply_preemptive_brake(lb, cfg.aln_floor);
    assert!(!brake);
}

/// Harness 3: For a simple, sorted calibration vector with small residual_mean,
/// the lower bound is finite and does not spuriously trigger Brake when
/// residuals are mild.
///
/// This guards against pathological behavior of the quantile selection.
#[kani::proof]
fn kani_ev_conformal_well_behaved_lower_bound() {
    let mut ev = baseline_ev();
    // Allow Kani to explore a small residual_mean within [-0.1, 0.1].
    let rm: f64 = kani::any();
    kani::assume(rm >= -0.1 && rm <= 0.1);
    ev.residual_mean = rm;

    // Fixed simple calibration scores.
    let calib_scores = [0.0_f64, 0.05_f64, 0.10_f64];

    let cfg = ConformalConfig {
        alpha: 0.2,
        aln_floor: -0.2, // floor lower than expected residuals
    };

    let lb = conformal_lower_bound(&ev, &calib_scores, &cfg);

    // Sanity: lb must be finite and reasonably bounded.
    assert!(lb.is_finite());
    assert!(lb > -10.0);
    assert!(lb < 10.0);

    // When lb comfortably above floor, Brake should be false.
    if lb >= cfg.aln_floor {
        let brake = apply_preemptive_brake(lb, cfg.aln_floor);
        assert!(!brake);
    }
}
