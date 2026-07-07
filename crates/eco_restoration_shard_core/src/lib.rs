// Filename: crates/eco_restoration_shard_core/src/lib.rs

//! Eco Restoration Shard Core
//!
//! Purpose:
//!   - Authoritative Rust core for qpudatashard handling in eco-restoration systems.
//!   - Computes K/E/R scores, Lyapunov residual V_t, and corridor status.
//!   - Exposes a narrow JSON IPC surface for Android/Kotlin apps and other clients.
//!   - Enforces non-actuating governance: no direct actuator commands.
//!
//! This crate is designed to integrate with EcoNetSchemaShard2026v1 and
//! ecosafety.riskvector/corridors grammar as described in your ecosystem docs.[file:29][file:114]

use serde::{Deserialize, Serialize};

/// Canonical qpudatashard representation in the core.
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
    pub corridor_status: String, // JSON summary of safe/gold/hard per coordinate
    pub evidencehex: String,
}

/// Eco-safety corridor bands for one coordinate (normalized).
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct CorridorBands {
    pub safe_min: f64,
    pub safe_max: f64,
    pub gold_min: f64,
    pub gold_max: f64,
    pub hard_min: f64,
    pub hard_max: f64,
}

/// Simple KER scoring bundle.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct KerScores {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

/// IPC request types from external clients (Android app, edge services).
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

/// IPC response variants.
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

/// Compute Lyapunov residual V_t from normalized risk coordinates.
/// In production, this should match ecosafety core's quadratic residual.[file:29]
pub fn compute_vt(
    energy_risk: f64,
    hydraulics_risk: f64,
    biology_risk: f64,
    carbon_risk: f64,
    materials_risk: f64,
    dataquality_risk: f64,
) -> f64 {
    let w_e = 0.15;
    let w_h = 0.20;
    let w_b = 0.20;
    let w_c = 0.15;
    let w_m = 0.15;
    let w_d = 0.15;

    w_e * energy_risk * energy_risk +
    w_h * hydraulics_risk * hydraulics_risk +
    w_b * biology_risk * biology_risk +
    w_c * carbon_risk * carbon_risk +
    w_m * materials_risk * materials_risk +
    w_d * dataquality_risk * dataquality_risk
}

/// Simple corridor classification for one coordinate.
pub fn classify_corridor(value: f64, bands: CorridorBands) -> &'static str {
    if value < bands.safe_min || value > bands.hard_max {
        "breach"
    } else if value >= bands.safe_min && value <= bands.safe_max {
        "safe"
    } else if value > bands.safe_max && value <= bands.gold_max {
        "gold"
    } else if value > bands.gold_max && value <= bands.hard_max {
        "hard"
    } else {
        "unknown"
    }
}

/// Compute K/E/R scores from normalized risks and V_t.
/// Placeholder scoring; replace with full KER grammar in production.[file:29]
pub fn compute_ker(vt: f64) -> KerScores {
    // Knowledge K ~ 0.90–0.95, Eco-impact E ~ 0.88–0.93, Risk R ~ normalized V_t.
    let k = 0.93;
    let e = 0.90;
    let r = (vt).min(1.0).max(0.0);
    KerScores { k, e, r }
}

/// Serialize corridor status summary as JSON string for clients.
pub fn corridor_status_json(
    energy_status: &str,
    hydraulics_status: &str,
    biology_status: &str,
    carbon_status: &str,
    materials_status: &str,
    dataquality_status: &str,
) -> String {
    let obj = serde_json::json!({
        "energy": energy_status,
        "hydraulics": hydraulics_status,
        "biology": biology_status,
        "carbon": carbon_status,
        "materials": materials_status,
        "dataquality": dataquality_status,
    });
    serde_json::to_string(&obj).unwrap_or_else(|_| "{}".to_string())
}

/// Handle an incoming IPC request and return a JSON response value.
///
/// In practice, this would read/write from an internal database of qpudatashards
/// and maintenance events. Here we show the type-safe surface.[file:66]
pub fn handle_request(req: AppRequest) -> Result<serde_json::Value, String> {
    match req {
        AppRequest::FetchShard { node_id } => {
            // Look up latest shard (placeholder/example; replace with DB lookup).
            let energy_risk = 0.3;
            let hydraulics_risk = 0.2;
            let biology_risk = 0.25;
            let carbon_risk = 0.15;
            let materials_risk = 0.20;
            let dataquality_risk = 0.10;

            let vt = compute_vt(
                energy_risk,
                hydraulics_risk,
                biology_risk,
                carbon_risk,
                materials_risk,
                dataquality_risk,
            );
            let ker = compute_ker(vt);

            let bands = CorridorBands {
                safe_min: 0.0,
                safe_max: 0.3,
                gold_min: 0.3,
                gold_max: 0.6,
                hard_min: 0.6,
                hard_max: 1.0,
            };

            let energy_status = classify_corridor(energy_risk, bands);
            let hydraulics_status = classify_corridor(hydraulics_risk, bands);
            let biology_status = classify_corridor(biology_risk, bands);
            let carbon_status = classify_corridor(carbon_risk, bands);
            let materials_status = classify_corridor(materials_risk, bands);
            let dataquality_status = classify_corridor(dataquality_risk, bands);

            let corridor_status = corridor_status_json(
                energy_status,
                hydraulics_status,
                biology_status,
                carbon_status,
                materials_status,
                dataquality_status,
            );

            let resp = AppResponse::Shard {
                node_id,
                window_start_ts: "2026-07-06T09:00:00Z".to_string(),
                window_end_ts: "2026-07-06T09:15:00Z".to_string(),
                ker_k: ker.k,
                ker_e: ker.e,
                ker_r: ker.r,
                vt,
                corridor_status,
                evidencehex: "0xa1b2c3d4e5f6".to_string(),
            };

            Ok(serde_json::to_value(resp).map_err(|e| e.to_string())?)
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
            // Record maintenance event and generate core evidence hex and KER impact.
            // In a full implementation, these values would depend on DB state.
            let core_evidencehex = format!(
                "0xcf34e1a2b3c4-{}-{}-{}",
                node_id, event_ts, device_id
            );
            let ker_impact_delta_k = 0.01;
            let ker_impact_delta_e = 0.00;
            let ker_impact_delta_r = -0.01;

            let _ignored = (engineer_id, event_type, notes, photo_uri, local_evidencehex);

            let resp = AppResponse::MaintenanceAck {
                status: "ok".to_string(),
                core_evidencehex,
                ker_impact_delta_k,
                ker_impact_delta_e,
                ker_impact_delta_r,
            };

            Ok(serde_json::to_value(resp).map_err(|e| e.to_string())?)
        }
    }
}
