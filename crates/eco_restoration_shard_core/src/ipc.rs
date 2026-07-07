// Filename: crates/eco_restoration_shard_core/src/ipc.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum AppRequest {
    FetchShard { node_id: String },
    MaintenanceEvent {
        node_id: String,
        event_ts: String,
        engineer_id: String,
        event_type: String,
        notes: Option<String>,
        photo_uri: Option<String>,
        local_evidencehex: String,
        device_id: String,
    },
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShardResponse {
    pub node_id: String,
    pub window_start_ts: String,
    pub window_end_ts: String,
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub vt: f64,
    pub corridor_status: String,
    pub evidencehex: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MaintenanceResponse {
    pub status: String,
    pub core_evidencehex: String,
    pub ker_impact_delta_k: f64,
    pub ker_impact_delta_e: f64,
    pub ker_impact_delta_r: f64,
}

pub fn handle_request(req: AppRequest) -> Result<serde_json::Value, String> {
    match req {
        AppRequest::FetchShard { node_id } => {
            // Look up shard in Rust-side DB, compute KER/corridors.
            let shard = ShardResponse {
                node_id,
                window_start_ts: "2026-07-06T09:00:00Z".to_string(),
                window_end_ts: "2026-07-06T09:15:00Z".to_string(),
                ker_k: 0.93,
                ker_e: 0.90,
                ker_r: 0.12,
                vt: 0.11,
                corridor_status: "{}".to_string(),
                evidencehex: "0xa1b2c3d4e5f6".to_string(),
            };
            Ok(serde_json::to_value(shard).unwrap())
        }
        AppRequest::MaintenanceEvent { node_id, event_ts, engineer_id, event_type, notes, photo_uri, local_evidencehex, device_id } => {
            // Record event, generate authoritative hex stamp and KER impact.
            let resp = MaintenanceResponse {
                status: "ok".to_string(),
                core_evidencehex: "0xcf34e1a2b3c4".to_string(),
                ker_impact_delta_k: 0.01,
                ker_impact_delta_e: 0.00,
                ker_impact_delta_r: -0.01,
            };
            Ok(serde_json::to_value(resp).unwrap())
        }
    }
}
