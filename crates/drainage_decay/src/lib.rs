// File: crates/drainage_decay/src/lib.rs

#![forbid(unsafe_code)]

//! Drainage Decay diagnostic crate.
//!
//! Non-actuating kernels for:
//! - BOD/TSS/CEC decay and duty window evaluation.
//! - EV signal integrity and conformal lower-bound + Brake logic.
//!
//! All functions are corridor-governed and suitable for Kani proofs.

pub mod lifeforce_duty;
pub mod ev_conformal;

pub use lifeforce_duty::{
    DrainageDutyDecision,
    DrainageRiskParams,
    DrainageStateSnapshot,
    DecayKernelParams,
    evaluate_duty_window,
};

pub use ev_conformal::{
    ConformalConfig,
    EvSignalIntegritySummary,
    apply_preemptive_brake,
    conformal_lower_bound,
};
