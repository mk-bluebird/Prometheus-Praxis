// FILE: crates/prometheus_praxis/src/ecorestoration/gaia_thresholds.rs
// ROLE: Gaia Sentinel auto-pause thresholds and consecutive-breach policy.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use rust_decimal::Decimal;

use super::gaia_snapshot::GaiaSentinelSnapshot;
use super::gaia_snapshot::AutopauseReason;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GaiaCorridorThresholds {
    pub policy_id: String,
    pub jurisdiction: String,
    pub soil_moisture_pause_threshold: Decimal,
    pub soil_moisture_days_below_for_pause: i32,
    pub heat_budget_pause_threshold: Decimal,
    pub drought_idx_pause_threshold: Decimal,
    pub flood_risk_pause_threshold: Decimal,
    pub fire_risk_pause_threshold: Decimal,
    pub max_consecutive_moisture_breach_days: i32,
    pub max_consecutive_heat_drought_breach_days: i32,
    pub max_consecutive_flood_breach_events: i32,
    pub max_consecutive_fire_breach_events: i32,
    pub preflight_rule_id: String,
    pub target_workflow_ids: Vec<String>,
}

impl GaiaCorridorThresholds {
    /// Pure guard: compute autopause reason for a single snapshot.
    pub fn autopause_reason_for(&self, snap: &GaiaSentinelSnapshot) -> AutopauseReason {
        let mut reasons = Vec::new();

        if snap.soil_moisture_idx < self.soil_moisture_pause_threshold {
            reasons.push(AutopauseReason::Moisture);
        }
        if snap.heat_budget_idx > self.heat_budget_pause_threshold
            && snap.drought_idx > self.drought_idx_pause_threshold
        {
            reasons.push(AutopauseReason::HeatDrought);
        }
        if snap.flood_risk_idx > self.flood_risk_pause_threshold {
            reasons.push(AutopauseReason::Flood);
        }
        if snap.fire_risk_idx > self.fire_risk_pause_threshold {
            reasons.push(AutopauseReason::Fire);
        }

        match reasons.len() {
            0 => AutopauseReason::None,
            1 => reasons[0].clone(),
            _ => AutopauseReason::Combined,
        }
    }
}
