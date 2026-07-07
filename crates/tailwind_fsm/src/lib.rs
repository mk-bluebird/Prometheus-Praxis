// Filename: crates/tailwind_fsm/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum TailwindState {
    Cold,
    CheckEnergy,
    CheckBio,
    Armed,
    Active,
    Fault,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct CorridorPredicates {
    pub biosurface_ok: bool,
    pub hydraulic_ok: bool,
    pub lyapunov_ok: bool,
    pub tailwind_valid: bool,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct NodeTelemetry {
    pub vt_prev: f64,
    pub vt_next_est: f64,
    pub energy_surplus_j: f64,
}

impl NodeTelemetry {
    pub fn vt_trend_non_positive(&self) -> bool {
        self.vt_next_est <= self.vt_prev
    }

    pub fn energy_surplus_positive(&self) -> bool {
        self.energy_surplus_j > 0.0
    }
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct TailwindFsm {
    pub state: TailwindState,
}

impl TailwindFsm {
    pub fn new() -> Self {
        Self { state: TailwindState::Cold }
    }

    pub fn step(
        &mut self,
        preds: CorridorPredicates,
        tel: NodeTelemetry,
    ) {
        self.state = match self.state {
            TailwindState::Cold => {
                if tel.energy_surplus_positive() {
                    TailwindState::CheckEnergy
                } else {
                    TailwindState::Fault
                }
            }
            TailwindState::CheckEnergy => {
                if tel.energy_surplus_positive() && preds.tailwind_valid {
                    TailwindState::CheckBio
                } else {
                    TailwindState::Fault
                }
            }
            TailwindState::CheckBio => {
                if preds.biosurface_ok && preds.hydraulic_ok {
                    TailwindState::Armed
                } else {
                    TailwindState::Fault
                }
            }
            TailwindState::Armed => {
                if preds.lyapunov_ok && tel.vt_trend_non_positive() {
                    TailwindState::Active
                } else {
                    TailwindState::Fault
                }
            }
            TailwindState::Active => {
                // Once active, remain active as long as invariants hold,
                // otherwise go to Fault.
                if preds.lyapunov_ok
                    && preds.biosurface_ok
                    && preds.hydraulic_ok
                    && preds.tailwind_valid
                    && tel.energy_surplus_positive()
                    && tel.vt_trend_non_positive()
                {
                    TailwindState::Active
                } else {
                    TailwindState::Fault
                }
            }
            TailwindState::Fault => TailwindState::Fault,
        };
    }
}
