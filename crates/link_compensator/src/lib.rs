#![forbid(unsafe_code)]

use anyhow::Result;
use chrono::{DateTime, Utc};
use rusqlite::{Connection, params};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LinkResolutionLogRow {
    pub id: i64,
    pub url: String,
    pub fetched_status: String,
    pub source_kind: String,
    pub archive_url: Option<String>,
    pub snapshot_ts_utc: Option<String>,
    pub eco_impact_score: f64,
    pub energy_saved_kwh: Option<f64>,
    pub co2_offset_kg: Option<f64>,
    pub material_recyclability: Option<String>,
    pub hex_proof: String,
    pub created_at_utc: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompensatedDatum {
    pub key: String,
    pub value: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LinkCompensationResult {
    pub log_row: LinkResolutionLogRow,
    pub data: Vec<CompensatedDatum>,
}

/// Classify URL into a known source_kind based on hostname and path.
pub fn classify_source_kind(url: &str) -> String {
    let lower = url.to_lowercase();
    if lower.contains("phoenix.gov") || lower.contains("phoenixopendata.com") {
        "CITY_GIS".to_string()
    } else if lower.contains("maricopa.gov") || lower.contains("data-maricopa.opendata.arcgis.com") {
        "COUNTY_GIS".to_string()
    } else if lower.contains("azgfd.com") {
        "STATE_WILDLIFE".to_string()
    } else if lower.contains("fws.gov") || lower.contains("ecos.fws.gov") {
        if lower.contains("wetlands") {
            "FED_WETLANDS".to_string()
        } else {
            "FED_CRIT_HABITAT".to_string()
        }
    } else {
        "UNKNOWN".to_string()
    }
}

/// Simple eco impact heuristic based on source_kind.
pub fn eco_impact_for_source(source_kind: &str) -> f64 {
    match source_kind {
        "CITY_GIS" => 0.70,
        "COUNTY_GIS" => 0.75,
        "STATE_WILDLIFE" => 0.82,
        "FED_CRIT_HABITAT" => 0.90,
        "FED_WETLANDS" => 0.88,
        _ => 0.50,
    }
}

/// Compute hex_proof as sha256(URL || "|" || status || "|" || source_kind || "|" || eco_score).
pub fn compute_hex_proof(url: &str, status: &str, source_kind: &str, eco_score: f64) -> String {
    let mut hasher = Sha256::new();
    let payload = format!("{}|{}|{}|{:.6}", url, status, source_kind, eco_score);
    hasher.update(payload.as_bytes());
    let digest = hasher.finalize();
    format!("0x{}", hex::encode(digest))
}

/// Insert a new resolution row and its compensated data.
pub fn insert_resolution(
    conn: &Connection,
    url: &str,
    fetched_status: &str,
    source_kind: &str,
    archive_url: Option<String>,
    snapshot_ts_utc: Option<DateTime<Utc>>,
    eco_impact_score: f64,
    energy_saved_kwh: Option<f64>,
    co2_offset_kg: Option<f64>,
    material_recyclability: Option<String>,
    data: Vec<CompensatedDatum>,
) -> Result<LinkCompensationResult> {
    let snapshot_str = snapshot_ts_utc.map(|dt| dt.to_rfc3339());
    let created_at = Utc::now().to_rfc3339();
    let hex_proof = compute_hex_proof(url, fetched_status, source_kind, eco_impact_score);

    conn.execute(
        r#"INSERT INTO link_resolution_log
        (url, fetched_status, source_kind, archive_url, snapshot_ts_utc,
         eco_impact_score, energy_saved_kwh, co2_offset_kg, material_recyclability,
         hex_proof, created_at_utc)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)"#,
        params![
            url,
            fetched_status,
            source_kind,
            archive_url,
            snapshot_str,
            eco_impact_score,
            energy_saved_kwh,
            co2_offset_kg,
            material_recyclability,
            hex_proof,
            created_at
        ],
    )?;

    let id = conn.last_insert_rowid();

    for d in &data {
        conn.execute(
            r#"INSERT INTO compensated_data_cache
            (resolution_id, key, value, created_at_utc)
            VALUES (?1, ?2, ?3, ?4)"#,
            params![id, d.key, d.value, &created_at],
        )?;
    }

    let row = LinkResolutionLogRow {
        id,
        url: url.to_string(),
        fetched_status: fetched_status.to_string(),
        source_kind: source_kind.to_string(),
        archive_url,
        snapshot_ts_utc: snapshot_str,
        eco_impact_score,
        energy_saved_kwh,
        co2_offset_kg,
        material_recyclability,
        hex_proof,
        created_at_utc: created_at,
    };

    Ok(LinkCompensationResult { log_row: row, data })
}
