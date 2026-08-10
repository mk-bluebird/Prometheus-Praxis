// File: crates/eco_restoration_shard_core/src/lib.rs
//! Eco Restoration Shard Core.
//!
//! Non-actuating qpudatashard state management, corridor evaluation, K/E/R
//! scoring, JSON IPC, materialized-report refresh, and MCP planner wiring.

use serde::{Deserialize, Serialize};
use std::{
    collections::BTreeMap,
    sync::{OnceLock, RwLock},
};

pub mod materialized_refresh;

pub mod mcp {
    pub mod eco_planner_tile_index;
}

pub use materialized_refresh::{refresh, run_periodically};

const WEIGHTS: [f64; 6] = [0.15, 0.20, 0.20, 0.15, 0.15, 0.15];

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct QpuDataShard {
    pub node_id: String,
    pub window_start_ts: String,
    pub window_end_ts: String,
    pub energy_risk: f64,
    pub hydraulics_risk: f64,
    pub biology_risk: f64,
    pub carbon_risk: f64,
    pub materials_risk: f64,
    pub dataquality_risk: f64,
    pub vt: f64,
    pub ker_k: f64,
    pub ker_e: f64,
    pub ker_r: f64,
    pub corridor_status: String,
    pub evidencehex: String,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct CorridorBands {
    pub safe_min: f64,
    pub safe_max: f64,
    pub gold_min: f64,
    pub gold_max: f64,
    pub hard_min: f64,
    pub hard_max: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct KerScores {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum AppRequest {
    FetchShard {
        node_id: String,
    },
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
#[serde(tag = "type")]
pub enum AppResponse {
    Shard {
        node_id: String,
        window_start_ts: String,
        window_end_ts: String,
        ker_k: f64,
        ker_e: f64,
        ker_r: f64,
        vt: f64,
        corridor_status: String,
        evidencehex: String,
    },
    MaintenanceAck {
        status: String,
        core_evidencehex: String,
        ker_impact_delta_k: f64,
        ker_impact_delta_e: f64,
        ker_impact_delta_r: f64,
    },
}

#[derive(Clone, Debug)]
struct MaintenanceRecord {
    node_id: String,
    event_ts: String,
    engineer_id: String,
    event_type: String,
    notes: Option<String>,
    photo_uri: Option<String>,
    evidencehex: String,
    device_id: String,
}

#[derive(Default)]
struct CoreState {
    shards: BTreeMap<String, QpuDataShard>,
    maintenance_records: Vec<MaintenanceRecord>,
}

static STATE: OnceLock<RwLock<CoreState>> = OnceLock::new();

fn state() -> &'static RwLock<CoreState> {
    STATE.get_or_init(|| RwLock::new(CoreState::default()))
}

fn bounded(value: f64) -> bool {
    value.is_finite() && (0.0..=1.0).contains(&value)
}

fn shard_risks(shard: &QpuDataShard) -> [f64; 6] {
    [
        shard.energy_risk,
        shard.hydraulics_risk,
        shard.biology_risk,
        shard.carbon_risk,
        shard.materials_risk,
        shard.dataquality_risk,
    ]
}

pub fn compute_vt(
    energy_risk: f64,
    hydraulics_risk: f64,
    biology_risk: f64,
    carbon_risk: f64,
    materials_risk: f64,
    dataquality_risk: f64,
) -> f64 {
    let risks = [
        energy_risk,
        hydraulics_risk,
        biology_risk,
        carbon_risk,
        materials_risk,
        dataquality_risk,
    ];
    WEIGHTS
        .iter()
        .zip(risks)
        .map(|(weight, risk)| weight * risk * risk)
        .sum()
}

pub fn compute_ker(vt: f64) -> KerScores {
    let residual_risk = vt.clamp(0.0, 1.0);
    KerScores {
        k: 1.0 - residual_risk,
        e: 1.0 - residual_risk,
        r: residual_risk,
    }
}

pub fn classify_corridor(value: f64, bands: CorridorBands) -> &'static str {
    if !value.is_finite()
        || bands.safe_min > bands.safe_max
        || bands.safe_max > bands.gold_min
        || bands.gold_min > bands.gold_max
        || bands.gold_max > bands.hard_min
        || bands.hard_min > bands.hard_max
    {
        return "invalid";
    }
    if value >= bands.safe_min && value <= bands.safe_max {
        "safe"
    } else if value >= bands.gold_min && value <= bands.gold_max {
        "gold"
    } else if value >= bands.hard_min && value <= bands.hard_max {
        "hard"
    } else {
        "breach"
    }
}

pub fn corridor_status_json(statuses: [&str; 6]) -> String {
    serde_json::json!({
        "energy": statuses[0],
        "hydraulics": statuses[1],
        "biology": statuses[2],
        "carbon": statuses[3],
        "materials": statuses[4],
        "dataquality": statuses[5],
    })
    .to_string()
}

pub fn validate_and_prepare_shard(shard: &mut QpuDataShard) -> Result<(), String> {
    if shard.node_id.trim().is_empty()
        || shard.window_start_ts.trim().is_empty()
        || shard.window_end_ts.trim().is_empty()
        || shard.evidencehex.trim().is_empty()
    {
        return Err("node, window bounds, and evidence are required".into());
    }

    let risks = shard_risks(shard);
    if risks.iter().any(|risk| !bounded(*risk)) {
        return Err("all risk coordinates must be finite values in [0,1]".into());
    }

    let bands = CorridorBands {
        safe_min: 0.0,
        safe_max: 0.30,
        gold_min: 0.30,
        gold_max: 0.60,
        hard_min: 0.60,
        hard_max: 1.0,
    };
    shard.vt = compute_vt(
        risks[0], risks[1], risks[2], risks[3], risks[4], risks[5],
    );
    let ker = compute_ker(shard.vt);
    shard.ker_k = ker.k;
    shard.ker_e = ker.e;
    shard.ker_r = ker.r;
    shard.corridor_status = corridor_status_json([
        classify_corridor(risks[0], bands),
        classify_corridor(risks[1], bands),
        classify_corridor(risks[2], bands),
        classify_corridor(risks[3], bands),
        classify_corridor(risks[4], bands),
        classify_corridor(risks[5], bands),
    ]);
    Ok(())
}

pub fn upsert_shard(mut shard: QpuDataShard) -> Result<(), String> {
    validate_and_prepare_shard(&mut shard)?;
    let mut guard = state().write().map_err(|_| "core state unavailable")?;
    guard.shards.insert(shard.node_id.clone(), shard);
    Ok(())
}

pub fn handle_request(request: AppRequest) -> Result<serde_json::Value, String> {
    match request {
        AppRequest::FetchShard { node_id } => {
            let guard = state().read().map_err(|_| "core state unavailable")?;
            let shard = guard
                .shards
                .get(&node_id)
                .ok_or_else(|| format!("no qpudatashard registered for node {node_id}"))?;

            serde_json::to_value(AppResponse::Shard {
                node_id: shard.node_id.clone(),
                window_start_ts: shard.window_start_ts.clone(),
                window_end_ts: shard.window_end_ts.clone(),
                ker_k: shard.ker_k,
                ker_e: shard.ker_e,
                ker_r: shard.ker_r,
                vt: shard.vt,
                corridor_status: shard.corridor_status.clone(),
                evidencehex: shard.evidencehex.clone(),
            })
            .map_err(|error| error.to_string())
        }
        AppRequest::MaintenanceEvent {
            node_id,
            event_ts,
            engineer_id,
            event_type,
            notes,
            photo_uri,
            local_evidencehex,
            device_id,
        } => {
            if node_id.trim().is_empty()
                || event_ts.trim().is_empty()
                || engineer_id.trim().is_empty()
                || event_type.trim().is_empty()
                || local_evidencehex.trim().is_empty()
                || device_id.trim().is_empty()
            {
                return Err("maintenance event contains required empty fields".into());
            }

            let mut guard = state().write().map_err(|_| "core state unavailable")?;
            if !guard.shards.contains_key(&node_id) {
                return Err(format!("cannot record maintenance for unknown node {node_id}"));
            }

            guard.maintenance_records.push(MaintenanceRecord {
                node_id,
                event_ts,
                engineer_id,
                event_type,
                notes,
                photo_uri,
                evidencehex: local_evidencehex.clone(),
                device_id,
            });

            serde_json::to_value(AppResponse::MaintenanceAck {
                status: "recorded".into(),
                core_evidencehex: local_evidencehex,
                ker_impact_delta_k: 0.0,
                ker_impact_delta_e: 0.0,
                ker_impact_delta_r: 0.0,
            })
            .map_err(|error| error.to_string())
        }
    }
}
