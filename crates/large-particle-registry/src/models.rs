use serde::{Deserialize, Serialize};
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LargeParticleFile {
    pub file_id: Uuid,
    pub file_hash: String,
    pub total_size_bytes: i64,
    pub chunk_size_hint: i32,
    pub content_type: String,
    pub summary_level_hint: String,
    pub ker_contribution: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LargeParticleBlock {
    pub block_id: Uuid,
    pub file_id: Uuid,
    pub block_index: i32,
    pub block_hash: String,
    pub size_bytes: i32,
    pub aggregate_json: serde_json::Value,
    pub offset: i64,
}
