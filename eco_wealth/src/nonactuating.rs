// filename: eco_wealth/src/nonactuating.rs

use crate::model::EcoWealthSnapshot;

/// Non-actuating eco-wealth kernel trait: pure computation over shard+spine state.
pub trait EcoWealthKernel {
    fn compute(&self) -> EcoWealthSnapshot;
}

/// Example helper that could be implemented by a NonActuatingWorkload-compatible crate.
/// This crate itself must not know about actuators.
pub fn compute_eco_wealth_snapshot(snapshot: EcoWealthSnapshot) -> EcoWealthSnapshot {
    snapshot
}
