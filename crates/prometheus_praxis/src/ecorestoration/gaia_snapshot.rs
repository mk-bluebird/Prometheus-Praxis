// FILE: crates/prometheus_praxis/src/ecorestoration/gaia_snapshot.rs
// ROLE: Real-time Gaia Sentinel snapshot feeding Governance Preflight.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use rust_decimal::Decimal;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum AutopauseReason {
    None,
    Moisture,
    HeatDrought,
    Flood,
    Fire,
    Combined,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GaiaSentinelSnapshot {
    pub snapshot_id: String,
    pub tile_id: String,
    pub timestamp_utc: String,
    pub soil_moisture_idx: Decimal,
    pub drought_idx: Decimal,
    pub heat_budget_idx: Decimal,
    pub flood_risk_idx: Decimal,
    pub fire_risk_idx: Decimal,
    pub moisture_below_floor: bool,
    pub heat_budget_over_limit: bool,
    pub drought_above_threshold: bool,
    pub flood_risk_high: bool,
    pub fire_risk_high: bool,
    pub corridor_violation_ids: Vec<String>,
    pub autopause_reason: AutopauseReason,
}
