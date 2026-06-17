// FILE: ecorestorationshard/src/lib.rs
// DESTINATION: ecorestorationshard/src/lib.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard

#![forbid(unsafe_code)]
#![warn(missing_docs)]

//! Ecorestorationshard top-level library.
//!
//! This crate provides a non-actuating index over EcoFort / EcoNet KER,
//! residual, EcoWealth, and governance artifacts. It is designed to
//! interact with the SQLite-based governance spine in read-only mode.

use std::path::Path;

use chrono::{DateTime, Utc};
use rusqlite::{Connection, OpenFlags};
use thiserror::Error;

/// Errors produced by the ecorestorationshard index.
#[derive(Debug, Error)]
pub enum ShardIndexError {
    /// Underlying SQLite error.
    #[error("SQLite error: {0}")]
    Sql(#[from] rusqlite::Error),

    /// Time parsing error.
    #[error("Time parse error: {0}")]
    TimeParse(#[from] chrono::ParseError),

    /// Generic configuration error.
    #[error("Configuration error: {0}")]
    Config(String),
}

/// Read-only handle to the EcoFort / EcoWealth governance database.
#[derive(Debug)]
pub struct ShardIndex {
    conn: Connection,
}

impl ShardIndex {
    /// Open a read-only connection to the governance database at `db_path`.
    ///
    /// The path must point to the canonical EcoFort spine DB containing
    /// `definitionregistry`, `planeweights`, `shardinstance`, and EcoWealth
    /// tables such as `StewardEcoWealthStatement`.
    pub fn open_readonly<P: AsRef<Path>>(db_path: P) -> Result<Self, ShardIndexError> {
        let conn = Connection::open_with_flags(
            db_path,
            OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_URI,
        )?;
        conn.pragma_update(None, "foreign_keys", "ON")?;
        Ok(Self { conn })
    }

    /// Return the number of active definitions in `definitionregistry`.
    ///
    /// This provides a quick health-check that the DB is wired and that
    /// the DefinitionRegistry has been seeded.
    pub fn active_definition_count(&self) -> Result<i64, ShardIndexError> {
        let mut stmt = self.conn.prepare(
            "SELECT COUNT(*) FROM definitionregistry WHERE active = 1",
        )?;
        let count: i64 = stmt.query_row([], |row| row.get(0))?;
        Ok(count)
    }

    /// A lightweight representation of a steward EcoWealth statement window.
    #[allow(clippy::struct_field_names)]
    pub fn list_steward_eco_wealth(
        &self,
        stewarddid: &str,
        limit: u32,
    ) -> Result<Vec<StewardEcoWealthRow>, ShardIndexError> {
        let sql = r#"
            SELECT
                stewarddid,
                region,
                lane,
                windowstartutc,
                windowendutc,
                kmean,
                emean,
                rmean,
                vtmaxwindow,
                ecounitfinal
            FROM StewardEcoWealthStatement
            WHERE stewarddid = ?
            ORDER BY windowendutc DESC
            LIMIT ?
        "#;

        let mut stmt = self.conn.prepare(sql)?;
        let mut rows = stmt.query((stewarddid, limit))?;
        let mut out = Vec::new();

        while let Some(row) = rows.next()? {
            let stewarddid: String = row.get(0)?;
            let region: String = row.get(1)?;
            let lane: String = row.get(2)?;
            let windowstartutc: String = row.get(3)?;
            let windowendutc: String = row.get(4)?;
            let kmean: f64 = row.get(5)?;
            let emean: f64 = row.get(6)?;
            let rmean: f64 = row.get(7)?;
            let vtmaxwindow: f64 = row.get(8)?;
            let ecounitfinal: f64 = row.get(9)?;

            out.push(StewardEcoWealthRow {
                stewarddid,
                region,
                lane,
                windowstartutc,
                windowendutc,
                kmean,
                emean,
                rmean,
                vtmaxwindow,
                ecounitfinal,
            });
        }

        Ok(out)
    }

    /// Cheap check that the EcoWealth statement window bounds are sane.
    ///
    /// Returns `true` if all rows have `windowstartutc <= windowendutc`.
    pub fn validate_eco_wealth_windows(&self) -> Result<bool, ShardIndexError> {
        let sql = r#"
            SELECT
                windowstartutc,
                windowendutc
            FROM StewardEcoWealthStatement
        "#;

        let mut stmt = self.conn.prepare(sql)?;
        let mut rows = stmt.query([])?;

        while let Some(row) = rows.next()? {
            let ws: String = row.get(0)?;
            let we: String = row.get(1)?;
            let ws_parsed: DateTime<Utc> = ws.parse()?;
            let we_parsed: DateTime<Utc> = we.parse()?;
            if we_parsed < ws_parsed {
                return Ok(false);
            }
        }

        Ok(true)
    }
}

/// Minimal view over a steward EcoWealth statement row.
///
/// This keeps the top-level crate focused on read-only analytics and
/// leaves detailed KER / residual math to the `kerresidual` crate.
#[derive(Debug, Clone)]
pub struct StewardEcoWealthRow {
    /// Steward DID (Bostrom address).
    pub stewarddid: String,
    /// Region code, e.g. "Phoenix-AZ-US".
    pub region: String,
    /// Lane label, e.g. "RESEARCH", "EXP", "PROD".
    pub lane: String,
    /// Window start timestamp in ISO-8601.
    pub windowstartutc: String,
    /// Window end timestamp in ISO-8601.
    pub windowendutc: String,
    /// Mean knowledge axis (K) over the window.
    pub kmean: f64,
    /// Mean ecological responsibility axis (E) over the window.
    pub emean: f64,
    /// Mean risk axis (R) over the window.
    pub rmean: f64,
    /// Maximum Lyapunov residual Vt over the window.
    pub vtmaxwindow: f64,
    /// Final EcoUnit scalar.
    pub ecounitfinal: f64,
}
