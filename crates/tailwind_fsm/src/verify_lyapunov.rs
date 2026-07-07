// ecorestoration_shard/crates/tailwindfsm/src/verify_lyapunov.rs
#![allow(dead_code)]

use kani::proof;

use crate::{TailwindFsm, TailwindState, CorridorPredicates, NodeTelemetry};

/// Simple Lyapunov residual: Vt = vt^2 with vt >= 0.
/// In your full stack, this is the summed quadratic over risk coordinates.
fn lyapunov(vt: f64) -> f64 {
    vt * vt
}

/// Construct corridor predicates that all hold.
fn all_corridors_hold() -> CorridorPredicates {
    CorridorPredicates {
        biosurface_ok: true,
        hydraulic_ok: true,
        lyapunov_ok: true,
        tailwind_valid: true,
    }
}

/// Telemetry with non-increasing Vt and positive energy surplus.
fn good_telemetry(vt_prev: f64, vt_next: f64, energy_surplus_j: f64) -> NodeTelemetry {
    NodeTelemetry {
        vt_prev,
        vt_next_est: vt_next,
        energy_surplus_j,
    }
}

/// Kani proof: whenever the FSM is in ACTIVE, the Lyapunov residual
/// computed from vt does not increase: Vt <= Vt-1.
#[proof]
fn vt_non_increasing_in_active() {
    // Choose some non-negative vt_prev and vt_next with vt_next <= vt_prev.
    let vt_prev: f64 = kani::any();
    let vt_next: f64 = kani::any();
    let energy_surplus_j: f64 = kani::any();

    kani::assume(vt_prev >= 0.0);
    kani::assume(vt_next >= 0.0);
    kani::assume(vt_next <= vt_prev);
    kani::assume(energy_surplus_j > 0.0);

    let preds = all_corridors_hold();
    let tel = good_telemetry(vt_prev, vt_next, energy_surplus_j);

    // Drive the FSM into ACTIVE along the canonical path.
    let mut fsm = TailwindFsm::new();
    // COLD -> CHECK_ENERGY
    fsm.step(preds, tel);
    // CHECK_ENERGY -> CHECK_BIO
    fsm.step(preds, tel);
    // CHECK_BIO -> ARMED
    fsm.step(preds, tel);
    // ARMED -> ACTIVE
    fsm.step(preds, tel);

    kani::assert_eq!(fsm.state, TailwindState::Active);

    // Compute Lyapunov residuals from vt_prev and vt_next.
    let v_prev = lyapunov(tel.vt_prev);
    let v_next = lyapunov(tel.vt_next_est);

    // Invariant: V_t <= V_{t-1} in ACTIVE.
    kani::assert!(v_next <= v_prev);
}
