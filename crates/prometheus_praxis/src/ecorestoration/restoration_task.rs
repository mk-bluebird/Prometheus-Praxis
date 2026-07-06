// FILE: crates/prometheus_praxis/src/ecorestoration/restoration_task.rs
// ROLE: Tile-level, corridor-aware restoration task for Nova Terra → daily sorties.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use rust_decimal::Decimal;

use crate::praxisgovernancekernel::ROHCEILING;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum ActionKind {
    Planting,
    SoilTreatment,
    WaterReroute,
    HabitatRestore,
    Monitoring,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RestorationTask {
    pub restoration_task_id: String,
    pub scenario_id: String,
    pub tile_id: String,
    pub birthsign_id: String,
    pub corridor_id: String,
    pub biotictreaty_id: String,
    pub action_kind: ActionKind,
    pub species_mix: Vec<String>,
    pub area_m2: f32,
    pub time_window_start: String,
    pub time_window_end: String,
    pub k_score: Decimal,
    pub e_score: Decimal,
    pub r_score: Decimal,
    pub gaia_snapshot_ref: String,
    pub boden_snapshot_ref: String,
}

impl RestorationTask {
    pub fn roh_within_global_ceiling(&self) -> bool {
        self.r_score <= ROHCEILING
    }
}
