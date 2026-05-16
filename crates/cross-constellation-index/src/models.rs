use serde::{Deserialize, Serialize};
use uuid::Uuid;

use aln_core::Did;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum HttpMethod {
    GET,
    POST,
    PUT,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerBand {
    pub k_min: f64,
    pub k_max: f64,
    pub e_min: f64,
    pub e_max: f64,
    pub r_max: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InteropIndexEntry {
    pub entry_id: Uuid,
    pub eco_shard_id: String,
    pub external_constellation: String,
    pub api_endpoint: String,
    pub method: HttpMethod,
    pub mapping: serde_json::Value,
    pub ker_band: KerBand,
    pub trust_anchor_did: Did,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SyncEvent {
    pub event_id: Uuid,
    pub entry_id: Uuid,
    pub correlation_id: String,
    pub direction: String,
    pub initiated_by_did: Did,
}

pub trait SyncAdapter {
    fn push(&self, entry: &InteropIndexEntry, payload: &serde_json::Value) -> bool;
    fn pull(&self, entry: &InteropIndexEntry) -> Option<serde_json::Value>;
}
