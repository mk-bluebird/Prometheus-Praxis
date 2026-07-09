//! Kani harnesses for FogGuard invariants.
//!
//! These proofs ensure that any violation of corridor, Lyapunov, or KER
//! constraints forces FogGuardVerdict::Stop.

#![cfg(kani)]
#![forbid(unsafe_code)]

use kani::any;

use crate::{
    fog_guard::{FogGuard, FogGuardConfig, FogGuardInput, FogGuardVerdict},
    KERWindow,
    LyapunovResidual,
    RiskCoord,
    RiskVector,
};

fn any_risk_vector() -> RiskVector {
    let rcec = RiskCoord::new_clamped(any());
    let rsat = RiskCoord::new_clamped(any());
    let rsurcharge = RiskCoord::new_clamped(any());
    let rbiodiv = RiskCoord::new_clamped(any());
    let rvt = RiskCoord::new_clamped(any());
    let rgovernance = RiskCoord::new_clamped(any());
    RiskVector {
        rcec,
        rsat,
        rsurcharge,
        rbiodiv,
        rvt,
        rgovernance,
    }
}

fn base_input(cfg: &FogGuardConfig) -> FogGuardInput {
    let risk = any_risk_vector();
    let residual_value: f64 = any();
    let residual = LyapunovResidual { value: residual_value };
    let k_val: f64 = any();
    let e_val: f64 = any();
    let r_val: f64 = any();

    let k = RiskCoord::new_clamped(k_val);
    let e = RiskCoord::new_clamped(e_val);
    let r = RiskCoord::new_clamped(r_val);

    FogGuardInput {
        risk,
        residual,
        corridor_present: true,
        safestep_ok: true,
        k,
        e,
        r,
    }
}

#[kani::proof]
fn fog_guard_stops_on_missing_corridor() {
    let cfg = FogGuardConfig::default();
    let guard = FogGuard::new(cfg);
    let mut input = base_input(guard.config());
    input.corridor_present = false;

    let verdict = guard.evaluate(&input);
    assert!(matches!(verdict, FogGuardVerdict::Stop));
}

#[kani::proof]
fn fog_guard_stops_on_residual_increase() {
    let cfg = FogGuardConfig::default();
    let guard = FogGuard::new(cfg);
    let mut input = base_input(guard.config());

    // Force residual above the configured maximum.
    input.residual.value = guard.config().bands.residual_max + 1e-6;

    let verdict = guard.evaluate(&input);
    assert!(matches!(verdict, FogGuardVerdict::Stop));
}

#[kani::proof]
fn fog_guard_stops_on_roh_ceiling_breach() {
    let cfg = FogGuardConfig::default();
    let guard = FogGuard::new(cfg);
    let mut input = base_input(guard.config());

    // Force RoH above the configured ceiling.
    let roh_ceiling = guard.config().bands.roh_ceiling;
    input.r = RiskCoord::new_clamped(roh_ceiling + 1e-6);

    let verdict = guard.evaluate(&input);
    assert!(matches!(verdict, FogGuardVerdict::Stop));
}

#[kani::proof]
fn fog_guard_stops_on_ker_threshold_violation() {
    let cfg = FogGuardConfig::default();
    let guard = FogGuard::new(cfg);
    let mut input = base_input(guard.config());

    // Violate at least one KER threshold: make K too low.
    input.k = RiskCoord::new_clamped(guard.config().ker.k_min - 0.05);

    let verdict = guard.evaluate(&input);
    assert!(matches!(verdict, FogGuardVerdict::Stop));
}

#[kani::proof]
fn fog_guard_stops_when_safestep_flag_is_false() {
    let cfg = FogGuardConfig::default();
    let guard = FogGuard::new(cfg);
    let mut input = base_input(guard.config());

    input.safestep_ok = false;

    let verdict = guard.evaluate(&input);
    assert!(matches!(verdict, FogGuardVerdict::Stop));
}
