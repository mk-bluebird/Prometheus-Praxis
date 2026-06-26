// filename: ppx_minimal_identity_kernel/src/api.rs
// repo: eco_restoration_shard/ppx_minimal_identity_kernel/src/api.rs

use rusqlite::{params, Connection, Result as SqlResult};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PsychContinuityEvidenceView {
    pub evidence_id: i64,
    pub subject_did: String,
    pub from_state_id: i64,
    pub to_state_id: i64,
    pub metric_id: String,
    pub score: f64,
    pub measured_at_utc: String,
    pub notes: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NeurorightCorridorSpecView {
    pub id: String,
    pub context_tag: String,
    pub description: String,
    pub right_name: String,
    pub min_protection_level: f64,
    pub max_risk_tolerance: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemWellBeingComponentView {
    pub system_id: String,
    pub context_tag: String,
    pub component_name: String,
    pub value: f64,
    pub description: String,
    pub assessed_at_utc: String,
    pub notes: Option<String>,
}
