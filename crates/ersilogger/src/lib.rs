// filename: ecorestorationshard/crates/ersilogger/src/lib.rs
//! Non-actuating ERSI logger crate.
//!
//! Purpose:
//! - Provide a minimal, read-only-aligned helper for recording AI interaction
//!   metrics into an SQLite ERSI table.
//! - Metrics: tokens_used, compute_joules, kfactor, efactor, rfactor.
//! - Align with existing kerresidual spine and governance grammar:
//!   * K/E/R factors are pure diagnostics from KnowledgeFactorKernel and
//!     EcoImpactKernel / residual kernels.
//!   * No actuation, no routing, no lane changes.

use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use thiserror::Error;
use time::{format_description::well_known::Rfc3339, OffsetDateTime};

/// ERSILogError captures all failure modes for this crate.
#[derive(Debug, Error)]
pub enum ERSILogError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),
    #[error("Time error: {0}")]
    Time(#[from] time::error::Error),
    #[error("Invalid metric: {0}")]
    InvalidMetric(String),
}

/// ERSI row structure, aligned with EcoNet research spine naming.
/// This is a pure data object; no actuation or side effects.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ERSIRow {
    /// Stable identifier for the logical interaction/shard.
    pub ersi_id: String,
    /// Bostrom DID or compatible address for the steward / owner.
    pub steward_did: String,
    /// Logical repo or app context, e.g. "ecorestorationshard".
    pub repo: String,
    /// Optional lane tag, e.g. "RESEARCH" / "SIM".
    pub lane: String,
    /// Tokens used for this interaction (prompt + completion).
    pub tokens_used: i64,
    /// Estimated compute energy in joules for this interaction.
    pub compute_joules: f64,
    /// Normalized knowledge factor K in [0,1].
    pub kfactor: f64,
    /// Normalized eco-impact factor E in [0,1].
    pub efactor: f64,
    /// Normalized residual / risk factor R in [0,1].
    pub rfactor: f64,
    /// Optional free-form topic tag, e.g. "biodegradable-substrate".
    pub topic: String,
    /// UTC timestamp when this row was created.
    pub created_utc: String,
}

impl ERSIRow {
    /// Create a new ERSIRow with current UTC timestamp.
    ///
    /// This function validates K/E/R factors to be within [0,1] and tokens/compute non-negative.
    pub fn new(
        ersi_id: impl Into<String>,
        steward_did: impl Into<String>,
        repo: impl Into<String>,
        lane: impl Into<String>,
        tokens_used: i64,
        compute_joules: f64,
        kfactor: f64,
        efactor: f64,
        rfactor: f64,
        topic: impl Into<String>,
    ) -> Result<Self, ERSILogError> {
        if tokens_used < 0 {
            return Err(ERSILogError::InvalidMetric(
                "tokens_used must be >= 0".to_string(),
            ));
        }
        if compute_joules < 0.0 {
            return Err(ERSILogError::InvalidMetric(
                "compute_joules must be >= 0.0".to_string(),
            ));
        }
        if !(0.0..=1.0).contains(&kfactor) {
            return Err(ERSILogError::InvalidMetric(
                "kfactor must be in [0,1]".to_string(),
            ));
        }
        if !(0.0..=1.0).contains(&efactor) {
            return Err(ERSILogError::InvalidMetric(
                "efactor must be in [0,1]".to_string(),
            ));
        }
        if !(0.0..=1.0).contains(&rfactor) {
            return Err(ERSILogError::InvalidMetric(
                "rfactor must be in [0,1]".to_string(),
            ));
        }

        let now = OffsetDateTime::now_utc();
        let created_utc = now.format(&Rfc3339)?;

        Ok(ERSIRow {
            ersi_id: ersi_id.into(),
            steward_did: steward_did.into(),
            repo: repo.into(),
            lane: lane.into(),
            tokens_used,
            compute_joules,
            kfactor,
            efactor,
            rfactor,
            topic: topic.into(),
            created_utc,
        })
    }
}

/// Create the ERSI table if it does not exist.
///
/// Schema is non-actuating, aligned with EcoNet research-band patterns:
/// - CHECK constraints bound K/E/R to [0,1].
/// - No foreign keys to actuation tables.
/// - Primary key is ersi_id + created_utc for idempotent logging.
pub fn init_ersi_schema(conn: &Connection) -> Result<(), ERSILogError> {
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS ersi_log (
            ersi_id        TEXT NOT NULL,
            steward_did    TEXT NOT NULL,
            repo           TEXT NOT NULL,
            lane           TEXT NOT NULL,
            tokens_used    INTEGER NOT NULL CHECK(tokens_used >= 0),
            compute_joules REAL NOT NULL CHECK(compute_joules >= 0.0),
            kfactor        REAL NOT NULL CHECK(kfactor >= 0.0 AND kfactor <= 1.0),
            efactor        REAL NOT NULL CHECK(efactor >= 0.0 AND efactor <= 1.0),
            rfactor        REAL NOT NULL CHECK(rfactor >= 0.0 AND rfactor <= 1.0),
            topic          TEXT NOT NULL,
            created_utc    TEXT NOT NULL,
            PRIMARY KEY (ersi_id, created_utc)
        );

        CREATE INDEX IF NOT EXISTS idx_ersi_log_steward_time
            ON ersi_log (steward_did, created_utc);

        CREATE INDEX IF NOT EXISTS idx_ersi_log_repo_time
            ON ersi_log (repo, created_utc);

        CREATE INDEX IF NOT EXISTS idx_ersi_log_topic_time
            ON ersi_log (topic, created_utc);
        "#,
    )?;
    Ok(())
}

/// Insert a single ERSIRow into the ERSI table.
pub fn insert_ersi_row(conn: &Connection, row: &ERSIRow) -> Result<(), ERSILogError> {
    conn.execute(
        r#"
        INSERT INTO ersi_log (
            ersi_id,
            steward_did,
            repo,
            lane,
            tokens_used,
            compute_joules,
            kfactor,
            efactor,
            rfactor,
            topic,
            created_utc
        ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);
        "#,
        params![
            row.ersi_id,
            row.steward_did,
            row.repo,
            row.lane,
            row.tokens_used,
            row.compute_joules,
            row.kfactor,
            row.efactor,
            row.rfactor,
            row.topic,
            row.created_utc,
        ],
    )?;
    Ok(())
}

/// Convenience helper:
/// - Opens or creates the SQLite file at `db_path`.
/// - Ensures the ERSI schema exists.
/// - Inserts a freshly-constructed ERSIRow.
///
/// This is intended for lightweight integration in AI-chat entrypoints.
pub fn log_ersi_event(
    db_path: &str,
    ersi_id: impl Into<String>,
    steward_did: impl Into<String>,
    repo: impl Into<String>,
    lane: impl Into<String>,
    tokens_used: i64,
    compute_joules: f64,
    kfactor: f64,
    efactor: f64,
    rfactor: f64,
    topic: impl Into<String>,
) -> Result<(), ERSILogError> {
    let conn = Connection::open(db_path)?;
    init_ersi_schema(&conn)?;
    let row = ERSIRow::new(
        ersi_id,
        steward_did,
        repo,
        lane,
        tokens_used,
        compute_joules,
        kfactor,
        efactor,
        rfactor,
        topic,
    )?;
    insert_ersi_row(&conn, &row)?;
    Ok(())
}
