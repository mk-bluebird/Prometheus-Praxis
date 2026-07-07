// Filename: crates/tailwind_fsm/src/verify.rs
#![allow(dead_code)]

use kani::proof;
use crate::{TailwindFsm, TailwindState, CorridorPredicates, NodeTelemetry};

fn all_corridors_hold() -> CorridorPredicates {
    CorridorPredicates {
        biosurface_ok: true,
        hydraulic_ok: true,
        lyapunov_ok: true,
        tailwind_valid: true,
    }
}

fn good_telemetry() -> NodeTelemetry {
    NodeTelemetry {
        vt_prev: 0.10,
        vt_next_est: 0.09,
        energy_surplus_j: 1000.0,
    }
}

#[proof]
fn fsm_never_fault_when_corridors_hold() {
    let preds = all_corridors_hold();
    let tel = good_telemetry();

    let mut fsm = TailwindFsm::new();
    kani::assert_eq!(fsm.state, TailwindState::Cold);

    fsm.step(preds, tel);
    kani::assert_ne!(fsm.state, TailwindState::Fault);
    kani::assert_eq!(fsm.state, TailwindState::CheckEnergy);

    fsm.step(preds, tel);
    kani::assert_ne!(fsm.state, TailwindState::Fault);
    kani::assert_eq!(fsm.state, TailwindState::CheckBio);

    fsm.step(preds, tel);
    kani::assert_ne!(fsm.state, TailwindState::Fault);
    kani::assert_eq!(fsm.state, TailwindState::Armed);

    fsm.step(preds, tel);
    kani::assert_ne!(fsm.state, TailwindState::Fault);
    kani::assert_eq!(fsm.state, TailwindState::Active);
}

#[proof]
fn active_only_if_vt_non_positive_and_energy_positive() {
    let preds = all_corridors_hold();
    let tel = good_telemetry();

    let mut fsm = TailwindFsm::new();
    fsm.step(preds, tel);
    fsm.step(preds, tel);
    fsm.step(preds, tel);
    fsm.step(preds, tel);

    kani::assert_eq!(fsm.state, TailwindState::Active);
    kani::assert!(tel.vt_trend_non_positive());
    kani::assert!(tel.energy_surplus_positive());
}
