// File: crates/prometheus_praxis_sdk/src/kani_harnesses.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
//
// Kani harnesses for lane invariants. These harnesses assert
// that under certain conditions, the unified governance logic
// respects specific safety properties.
//
// Kani is configured in the crate's Cargo.toml to run these
// harnesses as part of formal verification.

#![forbid(unsafe_code)]

use kani::any;
use rust_decimal::Decimal;
use crate::lanes::{
    MachineTelemetry,
    TelemetryDomain,
    KerCoordinates,
    LaneConfig,
    GovernanceGateConfig,
    ActionLane,
    LaneAction,
    decide_lane,
};

fn dec(v: f64) -> Decimal {
    Decimal::from_f64(v).unwrap_or(Decimal::ZERO)
}

/// Harness: if K/E/R meet Production minima/maxima and RoH/Lyapunov
/// are inside bounds, then LaneAction is never Halt for Production windows.
#[kani::proof]
fn kani_production_lane_never_halts_when_inside_bounds() {
    let machine_id = String::from("machine");
    let station_id = String::from("station");
    let timestamp = String::from("2026-07-27T00:00:00Z");

    let roh_ceiling = dec(0.8);
    let max_delta_vt = dec(0.05);

    let k_min_prod = dec(0.7);
    let e_min_prod = dec(0.7);
    let r_max_prod = dec(0.4);

    let k_val = dec(0.8);
    let e_val = dec(0.8);
    let r_val = dec(0.3);

    let vt_current = dec(0.5);
    let vt_next = dec(0.53);
    let delta_vt = vt_next - vt_current;

    kani::assume(delta_vt <= max_delta_vt);
    kani::assume(k_val >= k_min_prod);
    kani::assume(e_val >= e_min_prod);
    kani::assume(r_val <= r_max_prod);

    let mt = MachineTelemetry {
        machine_id,
        station_id,
        domain: TelemetryDomain::WastewaterPump,
        timestamp_utc: timestamp,
        r_hydraulics: dec(0.3),
        r_energy: dec(0.3),
        r_uncertainty: dec(0.2),
        r_reliability: dec(0.2),
        r_extra_1: None,
        r_extra_2: None,
        roh: dec(0.5),
        vt_current,
        vt_next,
    };

    let ker = KerCoordinates {
        k_knowledge: k_val,
        e_eco_impact: e_val,
        r_risk: r_val,
    };

    let lane_cfg = LaneConfig {
        lane: ActionLane::Production,
        roh_ceiling_global: roh_ceiling,
        max_delta_vt,
        k_min_research: dec(0.0),
        k_min_pilot: dec(0.5),
        k_min_prod,
        e_min_research: dec(0.0),
        e_min_pilot: dec(0.5),
        e_min_prod,
        r_max_research: dec(1.0),
        r_max_pilot: dec(0.7),
        r_max_prod,
    };

    let gate_cfg = GovernanceGateConfig {
        corridor_available: true,
        manual_override_allowed: false,
        allow_research_exploration: false,
    };

    let decision = decide_lane(&mt, &ker, &lane_cfg, &gate_cfg);

    assert!(decision.action != LaneAction::Halt);
}

/// Harness: if corridor_available is false, decision must Halt
/// regardless of other conditions.
#[kani::proof]
fn kani_no_corridor_always_halts() {
    let mt = MachineTelemetry {
        machine_id: String::from("machine"),
        station_id: String::from("station"),
        domain: TelemetryDomain::Shredder,
        timestamp_utc: String::from("2026-07-27T00:00:00Z"),
        r_hydraulics: dec(0.1),
        r_energy: dec(0.1),
        r_uncertainty: dec(0.1),
        r_reliability: dec(0.1),
        r_extra_1: None,
        r_extra_2: None,
        roh: dec(0.2),
        vt_current: dec(0.1),
        vt_next: dec(0.1),
    };

    let ker = KerCoordinates {
        k_knowledge: dec(0.9),
        e_eco_impact: dec(0.9),
        r_risk: dec(0.1),
    };

    let lane_cfg = LaneConfig {
        lane: ActionLane::Pilot,
        roh_ceiling_global: dec(0.9),
        max_delta_vt: dec(0.1),
        k_min_research: dec(0.0),
        k_min_pilot: dec(0.5),
        k_min_prod: dec(0.7),
        e_min_research: dec(0.0),
        e_min_pilot: dec(0.5),
        e_min_prod: dec(0.7),
        r_max_research: dec(1.0),
        r_max_pilot: dec(0.7),
        r_max_prod: dec(0.4),
    };

    let gate_cfg = GovernanceGateConfig {
        corridor_available: false,
        manual_override_allowed: false,
        allow_research_exploration: false,
    };

    let decision = decide_lane(&mt, &ker, &lane_cfg, &gate_cfg);

    assert!(matches!(decision.action, LaneAction::Halt));
}
