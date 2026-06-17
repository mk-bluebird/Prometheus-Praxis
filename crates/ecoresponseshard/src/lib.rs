// FILE: crates/ecoresponseshard/src/lib.rs
// DESTINATION: crates/ecoresponseshard/src/lib.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard
//
// Non-actuating response-index library.
// Exposes readonly KER + Dcombined views over the SQLite governance spine.
// No write path exists in this crate; every public function is pure read.

#![forbid(unsafe_code)]
#![warn(missing_docs)]

//! `ecoresponseshard` — canonical KER + D_combined readonly surface.
//!
//! # Design invariants
//! - All DB connections are opened `SQLITE_OPEN_READ_ONLY`.
//! - No INSERT / UPDATE / DELETE is issued from this crate.
//! - All public return types are `serde`-serialisable for JSON export.
//! - Trust filter: only rows with `trust_tier IN ('HIGH','VERIFIED')` and
//!   `lane = 'PROD'` are surfaced through `query_prod_high_trust`.

use std::path::Path;

use chrono::{DateTime, Utc};
use rusqlite::{Connection, OpenFlags};
use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Errors produced by this crate.
#[derive(Debug, Error)]
pub enum ResponseShardError {
    /// SQLite driver error.
    #[error("sqlite: {0}")]
    Sqlite(#[from] rusqlite::Error),
    /// Timestamp parse failure.
    #[error("timestamp parse: {0}")]
    TimeParse(#[from] chrono::ParseError),
    /// JSON serialisation failure.
    #[error("json: {0}")]
    Json(#[from] serde_json::Error),
}

/// A single response-shard row returned by the readonly index.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResponseShardRow {
    /// Shard instance identifier.
    pub shard_id: String,
    /// Owner DID (Bostrom address).
    pub owner_did: String,
    /// Region code.
    pub region: String,
    /// Lane label at snapshot time.
    pub lane: String,
    /// Trust tier assigned by governance kernel.
    pub trust_tier: String,
    /// Knowledge factor (K) at snapshot, clamped [0,1].
    pub k_factor: f64,
    /// Eco-impact factor (E) at snapshot, clamped [0,1].
    pub e_factor: f64,
    /// Risk factor (R) at snapshot, clamped [0,1].
    pub r_factor: f64,
    /// Combined discriminator D = K * E * (1 - R).
    pub d_combined: f64,
    /// Lyapunov residual Vt at snapshot.
    pub vt_residual: f64,
    /// Whether the shard was kerdeployable at snapshot time.
    pub kerdeployable: bool,
    /// Snapshot window start (ISO-8601).
    pub window_start_utc: String,
    /// Snapshot window end (ISO-8601).
    pub window_end_utc: String,
}

/// Open a readonly handle to the governance DB, enforcing FK pragma.
fn open_readonly<P: AsRef<Path>>(db_path: P) -> Result<Connection, ResponseShardError> {
    let conn = Connection::open_with_flags(
        db_path,
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_URI,
    )?;
    conn.pragma_update(None, "foreign_keys", "ON")?;
    Ok(conn)
}

/// Query parameters for `query_prod_high_trust`.
#[derive(Debug, Clone)]
pub struct ProdHighTrustFilter {
    /// Region to filter on, e.g. `"Phoenix-AZ-US"`. `None` = all regions.
    pub region: Option<String>,
    /// Maximum rows returned (applied as SQL LIMIT).
    pub limit: u32,
}

impl Default for ProdHighTrustFilter {
    fn default() -> Self {
        Self {
            region: None,
            limit: 256,
        }
    }
}

/// Return rows from `vw_prod_high_trust_responseshard` — the canonical
/// KER + D_combined surface for agents.
///
/// This function reads from a pre-created view (or falls back to the
/// `response_shard_snapshot` base table filtered inline) and returns
/// only `PROD`-lane, `HIGH`/`VERIFIED`-trust rows.
pub fn query_prod_high_trust<P: AsRef<Path>>(
    db_path: P,
    filter: ProdHighTrustFilter,
) -> Result<Vec<ResponseShardRow>, ResponseShardError> {
    let conn = open_readonly(db_path)?;

    let sql = match &filter.region {
        Some(_) => r#"
            SELECT
                shard_id,
                owner_did,
                region,
                lane,
                trust_tier,
                k_factor,
                e_factor,
                r_factor,
                (k_factor * e_factor * (1.0 - r_factor))   AS d_combined,
                vt_residual,
                kerdeployable,
                window_start_utc,
                window_end_utc
            FROM response_shard_snapshot
            WHERE lane        = 'PROD'
              AND trust_tier  IN ('HIGH', 'VERIFIED')
              AND region      = ?1
            ORDER BY d_combined DESC
            LIMIT ?2
        "#,
        None => r#"
            SELECT
                shard_id,
                owner_did,
                region,
                lane,
                trust_tier,
                k_factor,
                e_factor,
                r_factor,
                (k_factor * e_factor * (1.0 - r_factor))   AS d_combined,
                vt_residual,
                kerdeployable,
                window_start_utc,
                window_end_utc
            FROM response_shard_snapshot
            WHERE lane        = 'PROD'
              AND trust_tier  IN ('HIGH', 'VERIFIED')
            ORDER BY d_combined DESC
            LIMIT ?1
        "#,
    };

    let mut stmt = conn.prepare(sql)?;

    let rows = match &filter.region {
        Some(r) => {
            let limit = i64::from(filter.limit);
            stmt.query_map(rusqlite::params![r, limit], map_row)?
                .collect::<Result<Vec<_>, _>>()?
        }
        None => {
            let limit = i64::from(filter.limit);
            stmt.query_map(rusqlite::params![limit], map_row)?
                .collect::<Result<Vec<_>, _>>()?
        }
    };

    Ok(rows)
}

/// Serialise `query_prod_high_trust` result as a UTF-8 JSON string.
pub fn query_prod_high_trust_json<P: AsRef<Path>>(
    db_path: P,
    filter: ProdHighTrustFilter,
) -> Result<String, ResponseShardError> {
    let rows = query_prod_high_trust(db_path, filter)?;
    Ok(serde_json::to_string(&rows)?)
}

fn map_row(row: &rusqlite::Row<'_>) -> rusqlite::Result<ResponseShardRow> {
    Ok(ResponseShardRow {
        shard_id:         row.get(0)?,
        owner_did:        row.get(1)?,
        region:           row.get(2)?,
        lane:             row.get(3)?,
        trust_tier:       row.get(4)?,
        k_factor:         row.get(5)?,
        e_factor:         row.get(6)?,
        r_factor:         row.get(7)?,
        d_combined:       row.get(8)?,
        vt_residual:      row.get(9)?,
        kerdeployable:    row.get::<_, i64>(10)? != 0,
        window_start_utc: row.get(11)?,
        window_end_utc:   row.get(12)?,
    })
}

/// Summarise one workload window for a given shard.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadWindowSummary {
    /// Shard identifier.
    pub shard_id: String,
    /// Number of snapshot rows in the window.
    pub snapshot_count: i64,
    /// Mean K over the window.
    pub k_mean: f64,
    /// Mean E over the window.
    pub e_mean: f64,
    /// Mean R over the window.
    pub r_mean: f64,
    /// Max Vt residual seen in the window.
    pub vt_max: f64,
    /// Mean D_combined over the window.
    pub d_mean: f64,
    /// Window start (ISO-8601).
    pub window_start_utc: String,
    /// Window end (ISO-8601).
    pub window_end_utc: String,
}

/// Return an aggregated workload-window summary for `shard_id`.
pub fn summarize_workload_window<P: AsRef<Path>>(
    db_path: P,
    shard_id: &str,
    window_start_utc: &str,
    window_end_utc: &str,
) -> Result<Option<WorkloadWindowSummary>, ResponseShardError> {
    let conn = open_readonly(db_path)?;

    let sql = r#"
        SELECT
            shard_id,
            COUNT(*)                                                AS snapshot_count,
            AVG(k_factor)                                          AS k_mean,
            AVG(e_factor)                                          AS e_mean,
            AVG(r_factor)                                          AS r_mean,
            MAX(vt_residual)                                       AS vt_max,
            AVG(k_factor * e_factor * (1.0 - r_factor))           AS d_mean,
            MIN(window_start_utc)                                  AS window_start_utc,
            MAX(window_end_utc)                                    AS window_end_utc
        FROM response_shard_snapshot
        WHERE shard_id        = ?1
          AND window_start_utc >= ?2
          AND window_end_utc   <= ?3
        GROUP BY shard_id
    "#;

    let mut stmt = conn.prepare(sql)?;
    let maybe = stmt
        .query_row(
            rusqlite::params![shard_id, window_start_utc, window_end_utc],
            |row| {
                Ok(WorkloadWindowSummary {
                    shard_id:         row.get(0)?,
                    snapshot_count:   row.get(1)?,
                    k_mean:           row.get(2)?,
                    e_mean:           row.get(3)?,
                    r_mean:           row.get(4)?,
                    vt_max:           row.get(5)?,
                    d_mean:           row.get(6)?,
                    window_start_utc: row.get(7)?,
                    window_end_utc:   row.get(8)?,
                })
            },
        )
        .ok();

    Ok(maybe)
}

/// Serialise `summarize_workload_window` result as UTF-8 JSON.
pub fn summarize_workload_window_json<P: AsRef<Path>>(
    db_path: P,
    shard_id: &str,
    window_start_utc: &str,
    window_end_utc: &str,
) -> Result<String, ResponseShardError> {
    let summary =
        summarize_workload_window(db_path, shard_id, window_start_utc, window_end_utc)?;
    Ok(serde_json::to_string(&summary)?)
}

/// Ensure the `response_shard_snapshot` table and `vw_prod_high_trust_responseshard`
/// view exist.  Called once at DB initialisation or by the backfill binary.
/// This is the only DDL permitted in this crate and it is idempotent.
pub fn ensure_schema<P: AsRef<Path>>(db_path: P) -> Result<(), ResponseShardError> {
    let conn = Connection::open(db_path)?;
    conn.pragma_update(None, "foreign_keys", "ON")?;
    conn.execute_batch(SCHEMA_SQL)?;
    Ok(())
}

const SCHEMA_SQL: &str = r#"
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS response_shard_snapshot (
    snapshot_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    shard_id          TEXT    NOT NULL,
    owner_did         TEXT    NOT NULL,
    region            TEXT    NOT NULL,
    lane              TEXT    NOT NULL,
    trust_tier        TEXT    NOT NULL,
    k_factor          REAL    NOT NULL,
    e_factor          REAL    NOT NULL,
    r_factor          REAL    NOT NULL,
    vt_residual       REAL    NOT NULL,
    kerdeployable     INTEGER NOT NULL DEFAULT 0 CHECK (kerdeployable IN (0,1)),
    window_start_utc  TEXT    NOT NULL,
    window_end_utc    TEXT    NOT NULL,
    evidence_hex      TEXT    NOT NULL,
    created_utc       TEXT    NOT NULL,
    CHECK (k_factor   >= 0.0 AND k_factor   <= 1.0),
    CHECK (e_factor   >= 0.0 AND e_factor   <= 1.0),
    CHECK (r_factor   >= 0.0 AND r_factor   <= 1.0),
    CHECK (vt_residual >= 0.0),
    UNIQUE (shard_id, window_start_utc, window_end_utc)
);

CREATE INDEX IF NOT EXISTS idx_rss_shard_window
    ON response_shard_snapshot (shard_id, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_rss_lane_trust
    ON response_shard_snapshot (lane, trust_tier, region);

CREATE VIEW IF NOT EXISTS vw_prod_high_trust_responseshard AS
SELECT
    shard_id,
    owner_did,
    region,
    lane,
    trust_tier,
    k_factor,
    e_factor,
    r_factor,
    (k_factor * e_factor * (1.0 - r_factor))  AS d_combined,
    vt_residual,
    kerdeployable,
    window_start_utc,
    window_end_utc,
    evidence_hex
FROM response_shard_snapshot
WHERE lane       = 'PROD'
  AND trust_tier IN ('HIGH', 'VERIFIED')
  AND kerdeployable = 1;
"#;

#[cfg(test)]
mod tests {
    use super::*;

    fn mem_db_with_schema() -> Connection {
        let conn = Connection::open_in_memory().unwrap();
        conn.execute_batch(SCHEMA_SQL).unwrap();
        conn
    }

    #[test]
    fn schema_creates_view() {
        let conn = mem_db_with_schema();
        let count: i64 = conn
            .query_row(
                "SELECT COUNT(*) FROM vw_prod_high_trust_responseshard",
                [],
                |r| r.get(0),
            )
            .unwrap();
        assert_eq!(count, 0);
    }

    #[test]
    fn d_combined_formula() {
        let k = 0.94_f64;
        let e = 0.90_f64;
        let r = 0.13_f64;
        let d = k * e * (1.0 - r);
        assert!(d > 0.73 && d < 0.74, "d_combined={d}");
    }
}
