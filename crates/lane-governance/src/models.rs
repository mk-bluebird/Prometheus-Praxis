use serde::{Deserialize, Serialize};
use uuid::Uuid;

use ecospine::KER;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaneStatus {
    pub lane_id: Uuid,
    pub kernel_region: String,
    pub k_aggregate: f64,
    pub e_aggregate: f64,
    pub r_aggregate: f64,
    pub residual_trend: f64,
    pub last_evidence_window: (String, String),
    pub admissible: bool,
    pub reward_multiplier: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaneGovernanceRule {
    pub id: Uuid,
    pub lane_id: Uuid,
    pub predicate: serde_json::Value,
    pub reward_multiplier: f64,
    pub updated_by_did: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaneEvidencePoint {
    pub ker: KER,
    pub residual: f64,
    pub timestamp: f64,
}
