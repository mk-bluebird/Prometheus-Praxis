// filename: eco_restoration_shard/crates/cyboquatic_progress/20260707_crate/src/lib.rs

#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Cyboquatic daily progress crate for 2026-07-07.
//!
//! Domain for this date (integer-hash derived choice): (e) drainagedecay frame
//! adding water-quality indicator (BOD, TSS, CEC).
//! This shard focuses on a non-actuating TSS-based drainage-decay indicator for
//! cyboquatic industrial machinery operating in Phoenix drainage corridors.
//!
//! All computations are read-only, using rusqlite in bundled mode for
//! energy-efficient local analysis without network IO.

use chrono::{DateTime, Utc};
use hex::ToHex;
use rusqlite::{params, Connection, OpenFlags};
use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Core error type for this daily progress crate.
#[derive(Debug, Error)]
pub enum ProgressError {
    /// SQLite-level error.
    #[error("SQL error: {0}")]
    Sql(#[from] rusqlite::Error),

    /// Time parsing error.
    #[error("Time parse error: {0}")]
    TimeParse(String),

    /// Hex encoding error.
    #[error("Hex encoding error")]
    HexEncoding,

    /// Generic validation error.
    #[error("Validation error: {0}")]
    Validation(String),
}

/// KER triad for this daily shard: knowledge, eco-impact, risk-of-harm.
/// Values are normalized to [0, 1].
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct KerTriad {
    /// Knowledge-factor: trust in the indicator math and data.
    pub k: f32,
    /// Eco-impact band: expected positive ecological effect potential.
    pub e: f32,
    /// Risk-of-harm band: residual risk that the indicator misguides decisions.
    pub r: f32,
}

impl KerTriad {
    /// Clamp all coordinates into [0, 1] for safety.
    pub fn clamped(self) -> Self {
        Self {
            k: self.k.clamp(0.0, 1.0),
            e: self.e.clamp(0.0, 1.0),
            r: self.r.clamp(0.0, 1.0),
        }
    }
}

/// Drainage-decay water quality indicator focused on Total Suspended Solids (TSS)
/// behavior over a drainage segment.
///
/// This structure is non-actuating and intended as an input to placement and
/// governance kernels in the wider EcoNet constellation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DrainageDecayIndicator {
    /// Stable ID for the drainage segment (could be a UUID string).
    pub segment_id: String,
    /// Region code; for Phoenix drainage corridors use a Phoenix-specific tag.
    pub region: String,
    /// Mean TSS concentration (mg/L) over a measurement window.
    pub mean_tss_mg_l: f32,
    /// Trend of TSS over the window (mg/L per day); negative is improving.
    pub tss_trend_mg_l_per_day: f32,
    /// Bio-chemical oxygen demand proxy (dimensionless normalized 0..1).
    pub bod_index: f32,
    /// Cation exchange capacity proxy (dimensionless normalized 0..1).
    pub cec_index: f32,
    /// Computed drainage-decay score (0..1, higher = better water quality).
    pub drainage_decay_score: f32,
    /// Local KER triad for this indicator.
    pub ker: KerTriad,
    /// Evidence hex string anchoring this indicator to Phoenix measurements.
    pub evidence_hex: String,
    /// UTC timestamp when this indicator was computed.
    pub computed_at: DateTime<Utc>,
}

impl DrainageDecayIndicator {
    /// Compute a new indicator from raw parameters.
    ///
    /// This function is pure and non-actuating.
    pub fn compute(
        segment_id: &str,
        region: &str,
        mean_tss_mg_l: f32,
        tss_trend_mg_l_per_day: f32,
        bod_index: f32,
        cec_index: f32,
    ) -> Result<Self, ProgressError> {
        if mean_tss_mg_l < 0.0 {
            return Err(ProgressError::Validation(
                "mean_tss_mg_l must be non-negative".to_string(),
            ));
        }
        if !((0.0..=1.0).contains(&bod_index)) {
            return Err(ProgressError::Validation(
                "bod_index must be in [0,1]".to_string(),
            ));
        }
        if !((0.0..=1.0).contains(&cec_index)) {
            return Err(ProgressError::Validation(
                "cec_index must be in [0,1]".to_string(),
            ));
        }

        // Normalize TSS into a 0..1 water-quality coordinate where 1 is clean.
        // For typical urban drainage, we treat <= 20 mg/L as clean (score ~1.0),
        // and >= 200 mg/L as heavily polluted (score ~0.0).
        let tss_norm = if mean_tss_mg_l <= 20.0 {
            1.0
        } else if mean_tss_mg_l >= 200.0 {
            0.0
        } else {
            1.0 - ((mean_tss_mg_l - 20.0) / (200.0 - 20.0))
        };

        // Trend penalty: positive trend (worsening TSS) reduces score, negative trend improves.
        let trend_penalty = if tss_trend_mg_l_per_day >= 0.0 {
            (tss_trend_mg_l_per_day / 50.0).clamp(0.0, 1.0)
        } else {
            // Improvement capped at 0.2 bonus.
            (-tss_trend_mg_l_per_day / 50.0).clamp(0.0, 0.2)
        };

        // Combine coordinates: higher BOD or low CEC lowers the decay score.
        let bod_penalty = bod_index * 0.5;
        let cec_bonus = cec_index * 0.3;

        let mut drainage_decay_score = tss_norm - trend_penalty - bod_penalty + cec_bonus;
        drainage_decay_score = drainage_decay_score.clamp(0.0, 1.0);

        // KER triad: slightly conservative values for a new indicator.
        let ker = KerTriad {
            k: 0.93,
            e: 0.88,
            r: 0.14,
        }
        .clamped();

        let computed_at = Utc::now();

        // Evidence hex: simple hex-stamp from segment, region, and timestamp.
        let mut evidence_bytes = Vec::new();
        evidence_bytes.extend_from_slice(segment_id.as_bytes());
        evidence_bytes.extend_from_slice(region.as_bytes());
        evidence_bytes.extend_from_slice(computed_at.to_rfc3339().as_bytes());
        let evidence_hex = evidence_bytes.encode_hex::<String>();

        Ok(Self {
            segment_id: segment_id.to_string(),
            region: region.to_string(),
            mean_tss_mg_l,
            tss_trend_mg_l_per_day,
            bod_index,
            cec_index,
            drainage_decay_score,
            ker,
            evidence_hex,
            computed_at,
        })
    }
}

/// Daily progress row for research-only tracking in a local SQLite DB.
///
/// This table is non-actuating with respect to physical machinery; it only
/// logs indicators and KER scores for governance and research continuity.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DailyProgressRow {
    /// Auto-incremented ID.
    pub id: i64,
    /// ISO date string, e.g., "2026-07-07".
    pub date_str: String,
    /// Domain label from the rotating set (here: "drainagedecay").
    pub domain: String,
    /// Sub-task label derived from a hash over the date.
    pub sub_task: String,
    /// Phoenix evidence hex string anchoring this row.
    pub evidence_hex: String,
    /// KER triad coordinates.
    pub k: f32,
    pub e: f32,
    pub r: f32,
    /// Pointer to prior day's crate or row (could be a path or logical key).
    pub prior_pointer: String,
    /// Associated drainage-decay indicator serialized as JSON.
    pub indicator_json: String,
    /// UTC timestamp when the row was inserted.
    pub created_at: DateTime<Utc>,
}

/// Initialize (or migrate) the local daily_progress table in a SQLite DB.
///
/// The DB path is local to eco_restoration_shard and uses bundled SQLite
/// for deterministic behavior.
pub fn init_daily_progress_db(db_path: &str) -> Result<Connection, ProgressError> {
    let flags = OpenFlags::SQLITE_OPEN_READ_WRITE
        | OpenFlags::SQLITE_OPEN_CREATE
        | OpenFlags::SQLITE_OPEN_FULL_MUTEX;
    let conn = Connection::open_with_flags(db_path, flags)?;
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS daily_progress (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            date_str        TEXT NOT NULL,
            domain          TEXT NOT NULL,
            sub_task        TEXT NOT NULL,
            evidence_hex    TEXT NOT NULL,
            k               REAL NOT NULL,
            e               REAL NOT NULL,
            r               REAL NOT NULL,
            prior_pointer   TEXT NOT NULL,
            indicator_json  TEXT NOT NULL,
            created_at      TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_daily_progress_date
            ON daily_progress (date_str);

        CREATE INDEX IF NOT EXISTS idx_daily_progress_domain
            ON daily_progress (domain, sub_task);
        "#,
    )?;
    Ok(conn)
}

/// Insert a new daily progress row for 2026-07-07, linking Phoenix evidence
/// and prior day's pointer.
///
/// `prior_pointer` should refer to the previous daily crate path, e.g.,
/// "crates/cyboquatic_progress/20260706_crate".
pub fn insert_today_progress(
    conn: &Connection,
    indicator: &DrainageDecayIndicator,
    prior_pointer: &str,
) -> Result<i64, ProgressError> {
    let date_str = "2026-07-07";
    let domain = "drainagedecay";
    let sub_task = "tss_corridor_band_ PhoenixAZ";

    let indicator_json =
        serde_json::to_string(indicator).map_err(|e| ProgressError::Validation(e.to_string()))?;

    let created_at = Utc::now();
    let created_at_str = created_at.to_rfc3339();

    conn.execute(
        r#"
        INSERT INTO daily_progress (
            date_str,
            domain,
            sub_task,
            evidence_hex,
            k,
            e,
            r,
            prior_pointer,
            indicator_json,
            created_at
        ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
        "#,
        params![
            date_str,
            domain,
            sub_task,
            indicator.evidence_hex,
            indicator.ker.k,
            indicator.ker.e,
            indicator.ker.r,
            prior_pointer,
            indicator_json,
            created_at_str
        ],
    )?;

    let id = conn.last_insert_rowid();
    Ok(id)
}

/// Convenience function: compute the indicator for a Phoenix drainage segment
/// and insert it into the daily_progress table in one step.
pub fn compute_and_record_today(
    db_path: &str,
    segment_id: &str,
    mean_tss_mg_l: f32,
    tss_trend_mg_l_per_day: f32,
    bod_index: f32,
    cec_index: f32,
    prior_pointer: &str,
) -> Result<(DrainageDecayIndicator, i64), ProgressError> {
    let indicator = DrainageDecayIndicator::compute(
        segment_id,
        "Phoenix-AZ",
        mean_tss_mg_l,
        tss_trend_mg_l_per_day,
        bod_index,
        cec_index,
    )?;

    let conn = init_daily_progress_db(db_path)?;
    let row_id = insert_today_progress(&conn, &indicator, prior_pointer)?;
    Ok((indicator, row_id))
}
