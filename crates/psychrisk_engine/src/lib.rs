// filepath: crates/psychrisk_engine/src/lib.rs
#![forbid(unsafe_code)]

pub mod types;
pub mod mental_integrity_guard;

pub use mental_integrity_guard::{
    AmberUpliftDecisionResult,
    AmberUpliftReason,
    MentalIntegrityGuardInputs,
    AdultFloorEnvelope,
    evaluate_amber_uplift,
};
