// filename: crates/ai_node_shard/src/lib.rs

//! AI Node qpudatashards and Phoenix Hex registry bindings.
//!
//! This crate is non-actuating. It provides:
//! - A typed `AINodeShard` struct for AI data centers as Cyboquatic nodes.
//! - A `PhoenixHexAnchor` schema and minimal registry helpers over SQLite.
//! - Glue types to plug into existing kercore / ersilogger stacks.

use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use serde_with::serde_as;
use thiserror::Error;
use time::{format_description::well_known::Rfc3339, OffsetDateTime};

use steward_identity::StewardIdentity;

/// AI node planes and metrics.
/// All values are per measurement window and intended for normalization into rx and Vt in kercore.
#[serde_as]
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AINodeShard {
    // Identity and topology
    pub nodeid: String,
    pub region: String,
    pub lane: String, // RESEARCH | EXP | PROD
    pub steward: StewardIdentity,

    // Core measurement window
    pub twindow_start: String, // ISO-8601
    pub twindow_end: String,   // ISO-8601

    // Energy and efficiency planes
    pub core_energy_kwh_per_workload: f64, // kWh / workload (tokens or inferences)
    pub joules_per_inference: f64,         // J / inference
    pub pue: f64,                          // Power Usage Effectiveness
    pub cue_kg_co2_per_kwh: f64,           // Carbon Usage Effectiveness
    pub eco_per_joule: f64,                // eco-benefit per joule (normalized to a corridor)

    // Bandwidth and utilization
    pub throughput_tokens_per_s: f64,
    pub throughput_inferences_per_s: f64,
    pub utilization_pct: f64, // 0–100%

    // Heat reuse and eco ratio
    pub ere: f64,                 // Energy Reuse Effectiveness
    pub eco_task_ratio_pct: f64,  // % of energy on eco-restorative workloads

    // Water and materials
    pub wue_l_per_kwh: f64,
    pub embodied_kg_co2eq: f64, // embodied carbon for construction / refresh amortized per window

    // Derived KER-like scores at node/window level
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub vt: f64, // Lyapunov residual over AI planes

    // Composite eco-strength index S (for ranking / routing only)
    pub strength_index_s: f64,

    // Evidence and registry bindings
    pub evidencehex: String,
    pub signinghex: String, // usually same as steward.signinghex
}

/// Error type for AI node shard and registry operations.
#[derive(Debug, Error)]
pub enum AINodeError {
    #[error("invalid measurement: {0}")]
    InvalidMeasurement(String),
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),
    #[error("time error: {0}")]
    Time(#[from] time::error::Error),
}

/// Minimal sanity checks on raw measurements before normalization.
///
/// This does not enforce corridors; that is delegated to kercore.
impl AINodeShard {
    pub fn validate(&self) -> Result<(), AINodeError> {
        if self.core_energy_kwh_per_workload < 0.0 {
            return Err(AINodeError::InvalidMeasurement(
                "core_energy_kwh_per_workload must be >= 0".into(),
            ));
        }
        if self.joules_per_inference < 0.0 {
            return Err(AINodeError::InvalidMeasurement(
                "joules_per_inference must be >= 0".into(),
            ));
        }
        if !(0.0..=100.0).contains(&self.utilization_pct) {
            return Err(AINodeError::InvalidMeasurement(
                "utilization_pct must be in [0,100]".into(),
            ));
        }
        if !(0.0..=1.0).contains(&self.k)
            || !(0.0..=1.0).contains(&self.e)
            || !(0.0..=1.0).contains(&self.r)
        {
            return Err(AINodeError::InvalidMeasurement(
                "KER factors must be in [0,1]".into(),
            ));
        }
        Ok(())
    }
}

/// Phoenix Hex registry anchor entry.
///
/// This mirrors the "phoenix_hex_anchor" SQLite table, providing an append-only
/// ledger of evidence_hex anchors for telemetry, shards, and corridor specs.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct PhoenixHexAnchor {
    pub hex_id: String,          // evidence_hex
    pub kind: String,            // TELEMETRY | SHARD | GOV_CORRIDOR | ...
    pub logical_name: String,    // e.g., "PHX_AI_ENERGY_DV_20260709"
    pub path: String,            // relative path within the repo
    pub steward_uuid: String,    // from StewardIdentity
    pub created_utc: String,     // ISO-8601
    pub prior_anchor_id: Option<String>, // previous hex_id in this chain
}

impl PhoenixHexAnchor {
    /// Create a new anchor with current UTC timestamp.
    pub fn new(
        hex_id: impl Into<String>,
        kind: impl Into<String>,
        logical_name: impl Into<String>,
        path: impl Into<String>,
        steward_uuid: impl Into<String>,
        prior_anchor_id: Option<String>,
    ) -> Result<Self, AINodeError> {
        let hex_id = hex_id.into();
        let kind = kind.into();
        let logical_name = logical_name.into();
        let path = path.into();
        let steward_uuid = steward_uuid.into();

        if hex_id.is_empty() {
            return Err(AINodeError::InvalidMeasurement(
                "hex_id must not be empty".into(),
            ));
        }
        if logical_name.is_empty() {
            return Err(AINodeError::InvalidMeasurement(
                "logical_name must not be empty".into(),
            ));
        }

        let now = OffsetDateTime::now_utc();
        let created_utc = now.format(&Rfc3339)?;

        Ok(Self {
            hex_id,
            kind,
            logical_name,
            path,
            steward_uuid,
            created_utc,
            prior_anchor_id,
        })
    }
}

/// Initialize the Phoenix Hex registry schema in a SQLite database.
///
/// This is non-actuating and safe to run in CI or local tooling.
pub fn init_phoenix_hex_schema(conn: &Connection) -> Result<(), AINodeError> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS phoenix_hex_anchor (
            hex_id          TEXT PRIMARY KEY,
            kind            TEXT NOT NULL,
            logical_name    TEXT NOT NULL,
            path            TEXT NOT NULL,
            steward_uuid    TEXT NOT NULL,
            created_utc     TEXT NOT NULL,
            prior_anchor_id TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_phx_hex_kind
            ON phoenix_hex_anchor(kind);

        CREATE INDEX IF NOT EXISTS idx_phx_hex_logical
            ON phoenix_hex_anchor(logical_name);

        CREATE INDEX IF NOT EXISTS idx_phx_hex_steward
            ON phoenix_hex_anchor(steward_uuid, created_utc);
        "#,
    )?;
    Ok(())
}

/// Insert a Phoenix Hex anchor into the registry (append-only).
pub fn insert_phoenix_hex_anchor(
    conn: &Connection,
    anchor: &PhoenixHexAnchor,
) -> Result<(), AINodeError> {
    conn.execute(
        r#"
        INSERT INTO phoenix_hex_anchor (
            hex_id,
            kind,
            logical_name,
            path,
            steward_uuid,
            created_utc,
            prior_anchor_id
        ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);
        "#,
        params![
            anchor.hex_id,
            anchor.kind,
            anchor.logical_name,
            anchor.path,
            anchor.steward_uuid,
            anchor.created_utc,
            anchor.prior_anchor_id,
        ],
    )?;
    Ok(())
}

/// Fetch the latest anchor in a logical chain (by created_utc).
pub fn latest_anchor_for_logical(
    conn: &Connection,
    logical_name: &str,
) -> Result<Option<PhoenixHexAnchor>, AINodeError> {
    let mut stmt = conn.prepare(
        r#"
        SELECT hex_id, kind, logical_name, path, steward_uuid, created_utc, prior_anchor_id
        FROM phoenix_hex_anchor
        WHERE logical_name = ?1
        ORDER BY created_utc DESC
        LIMIT 1;
        "#,
    )?;

    let mut rows = stmt.query(params![logical_name])?;
    if let Some(row) = rows.next()? {
        let hex_id: String = row.get(0)?;
        let kind: String = row.get(1)?;
        let logical_name: String = row.get(2)?;
        let path: String = row.get(3)?;
        let steward_uuid: String = row.get(4)?;
        let created_utc: String = row.get(5)?;
        let prior_anchor_id: Option<String> = row.get(6)?;

        Ok(Some(PhoenixHexAnchor {
            hex_id,
            kind,
            logical_name,
            path,
            steward_uuid,
            created_utc,
            prior_anchor_id,
        }))
    } else {
        Ok(None)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use steward_identity::StewardIdentity;

    const BOSTROM_DID: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
    const STEWARD_ID: &str = "wallet_fetch18sd2uj";
    const STEWARD_UUID: &str = "87cb8e02-c918-4b2a-aa40-36a8efa37e52";

    fn sample_steward() -> StewardIdentity {
        StewardIdentity::new(
            BOSTROM_DID,
            STEWARD_ID,
            STEWARD_UUID,
            "STEWARD",
            "RESEARCH",
        )
        .expect("valid identity")
    }

    #[test]
    fn ai_node_shard_validation_ok() {
        let shard = AINodeShard {
            nodeid: "AI-PHX-001".into(),
            region: "Phoenix-AZ".into(),
            lane: "RESEARCH".into(),
            steward: sample_steward(),
            twindow_start: "2026-07-16T00:00:00Z".into(),
            twindow_end: "2026-07-16T01:00:00Z".into(),
            core_energy_kwh_per_workload: 0.25,
            joules_per_inference: 3.5,
            pue: 1.15,
            cue_kg_co2_per_kwh: 0.20,
            eco_per_joule: 0.75,
            throughput_tokens_per_s: 1200.0,
            throughput_inferences_per_s: 35.0,
            utilization_pct: 76.0,
            ere: 0.10,
            eco_task_ratio_pct: 65.0,
            wue_l_per_kwh: 0.9,
            embodied_kg_co2eq: 15.0,
            k: 0.93,
            e: 0.91,
            r: 0.13,
            vt: 0.42,
            strength_index_s: 0.88,
            evidencehex: "0xa3f5c7e9b1d20468c7e4a9d2b5f81357".into(),
            signinghex: BOSTROM_DID.into(),
        };

        shard.validate().expect("valid shard");

        let json = serde_json::to_string_pretty(&shard).expect("serialize");
        let decoded: AINodeShard = serde_json::from_str(&json).expect("deserialize");

        assert_eq!(decoded.nodeid, "AI-PHX-001");
        assert_eq!(decoded.steward.steward_uuid, STEWARD_UUID);
    }

    #[test]
    fn phoenix_hex_registry_round_trip() {
        let conn = Connection::open_in_memory().expect("in memory db");
        init_phoenix_hex_schema(&conn).expect("init schema");

        let anchor = PhoenixHexAnchor::new(
            "0xa3f5c7e9b1d20468c7e4a9d2b5f81357",
            "SHARD",
            "PHX_AI_ENERGY_DV_20260716",
            "qpudatashards/particles/AINodePhoenix2026v1.csv",
            STEWARD_UUID,
            None,
        )
        .expect("anchor");

        insert_phoenix_hex_anchor(&conn, &anchor).expect("insert");

        let latest =
            latest_anchor_for_logical(&conn, "PHX_AI_ENERGY_DV_20260716").expect("fetch latest");

        assert!(latest.is_some());
        let latest = latest.unwrap();
        assert_eq!(latest.hex_id, anchor.hex_id);
        assert_eq!(latest.steward_uuid, STEWARD_UUID);
    }
}
