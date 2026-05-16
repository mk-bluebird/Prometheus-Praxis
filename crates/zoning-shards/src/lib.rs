// crates/zoning-shards/src/lib.rs
use serde::{Deserialize, Serialize};
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ZoningShard {
    pub shard_id: Uuid,
    pub region_id: String,
    pub zone_code: String,
    pub regulation: serde_json::Value,
    pub ker_weight: f64,
    pub effective_from: String,
    pub source_document_hash: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoAction {
    pub region_id: String,
    pub zone_code: String,
    pub action_type: String, // "tree_corridor", "wetland", etc.
    pub params: serde_json::Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComplianceReport {
    pub compliant: bool,
    pub messages: Vec<String>,
    pub ker_bonus: f64,
}

pub fn check_compliance(shard: &ZoningShard, action: &EcoAction) -> ComplianceReport {
    let mut compliant = true;
    let mut messages = Vec::new();
    let mut ker_bonus = 0.0;

    if shard.region_id != action.region_id || shard.zone_code != action.zone_code {
        compliant = false;
        messages.push("Region or zone mismatch".to_string());
    }

    if compliant {
        ker_bonus = shard.ker_weight;
        messages.push("Action compliant with zoning shard".to_string());
    }

    ComplianceReport {
        compliant,
        messages,
        ker_bonus,
    }
}
