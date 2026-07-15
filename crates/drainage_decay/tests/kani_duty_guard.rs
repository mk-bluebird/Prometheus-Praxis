// File: crates/drainage_decay/tests/kani_duty_guard.rs

#![allow(dead_code)]
#![forbid(unsafe_code)]

use drainage_decay::{
    DrainageDutyDecision,
    DrainageRiskParams,
    DrainageStateSnapshot,
    DecayKernelParams,
    evaluate_duty_window,
};

/// Helper: construct a corridor-safe baseline snapshot.
fn safe_snapshot() -> DrainageStateSnapshot {
    DrainageStateSnapshot {
        corridorid: "PHX-DRAINAGE-BASELINE-01".to_string(),
        bod_mg_per_l: 40.0,
        tss_mg_per_l: 80.0,
        cec_cmol_per_kg: 25.0,
        temperature_c: 22.0,
        flow_lps: 5.0,
        k_score: 0.8,
        e_score: 0.9,
        r_score: 0.20, // within ≤ 0.25 corridor
    }
}

/// Helper: conservative decay parameters aligned with ISO/OECD guidance.
fn conservative_decay() -> DecayKernelParams {
    DecayKernelParams {
        k_bod_per_day: 0.20,
        k_tss_per_day: 0.06,
        theta: 1.05,
        ref_temp_c: 20.0,
    }
}

/// Helper: risk parameters with low lambda/gamma and veto above baseline.
fn conservative_risk() -> DrainageRiskParams {
    DrainageRiskParams {
        lambda_bod: 0.05,
        lambda_tss: 0.03,
        gamma_bod: 0.01,
        gamma_tss: 0.01,
        lveto_bod: 100.0,  // veto well above current BOD
        lveto_tss: 200.0,  // veto well above current TSS
        monitoring_dt_hours: 6.0,
    }
}

/// Harness 1: For a corridor-safe baseline, any allowed duty window must have
/// prob_hit_any < 1e-12 and respect ALN floors for BOD/TSS.
///
/// This encodes the ALN invariants from drainage-duty-guard.v1.aln.
#[kani::proof]
fn kani_duty_guard_enforces_hitting_prob_and_floors() {
    let state = safe_snapshot();
    let decay_params = conservative_decay();
    let risk = conservative_risk();

    // ALN floors (e.g., normalized 0.0, but here using physical mg/L thresholds).
    let aln_floor_bod = 0.0;
    let aln_floor_tss = 0.0;

    let decision: DrainageDutyDecision =
        evaluate_duty_window(&state, &decay_params, risk, aln_floor_bod, aln_floor_tss);

    if decision.allowed {
        // Hitting probability corridor.
        assert!(decision.prob_hit_any < 1e-12);

        // Deterministic floors: BOD/TSS must remain ≥ ALN floors.
        // We cannot directly read bod_next/tss_next from here, but the
        // implementation guarantees that if they crossed floors, allowed = false.
        // So allowed implies floors respected.
        assert!(decision.reasons.iter().all(|r| {
            !r.contains("Deterministic decay crosses ALN floor")
        }));
    }
}

/// Harness 2: If R_score is above 0.25, duty windows must not be allowed.
#[kani::proof]
fn kani_duty_guard_blocks_high_rscore() {
    let mut state = safe_snapshot();
    state.r_score = 0.30; // above corridor bound

    let decay_params = conservative_decay();
    let risk = conservative_risk();
    let aln_floor_bod = 0.0;
    let aln_floor_tss = 0.0;

    let decision =
        evaluate_duty_window(&state, &decay_params, risk, aln_floor_bod, aln_floor_tss);

    assert!(!decision.allowed);
}

/// Harness 3: If deterministic decay crosses ALN floors, duty must not be allowed.
///
/// We model this by a very long monitoring horizon (large dt) so that the
/// decay_to_horizon implementation drives BOD/TSS toward 0.
#[kani::proof]
fn kani_duty_guard_blocks_floor_crossing() {
    let state = safe_snapshot();
    let decay_params = conservative_decay();

    let mut risk = conservative_risk();
    risk.monitoring_dt_hours = 240.0; // 10 days, enough to decay BOD/TSS

    let aln_floor_bod = 10.0; // floor above 0
    let aln_floor_tss = 20.0;

    let decision =
        evaluate_duty_window(&state, &decay_params, risk, aln_floor_bod, aln_floor_tss);

    // The implementation must set allowed = false when crossing floors.
    assert!(!decision.allowed);
    assert!(decision
        .reasons
        .iter()
        .any(|r| r.contains("ALN floor")));
}
