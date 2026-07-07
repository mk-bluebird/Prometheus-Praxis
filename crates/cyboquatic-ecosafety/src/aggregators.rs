// filename: cyboquatic_index/src/aggregators.rs
// destination: cyboquatic_index/src/aggregators.rs

#![forbid(unsafe_code)]

use alloc::string::String;
use alloc::vec::Vec;

use rusqlite::{params, Connection};

/// Snapshot of ecosafety state for a given window.
#[derive(Debug, Clone)]
pub struct EcosafetyNodeStatus {
    pub node_id: String,
    pub window_start_utc: String,
    pub window_end_utc: String,
    pub lane: String,
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub vt: f32,
    pub roh: f32,
    pub ecosafety_state: String,
}

/// Aggregated trend view over a time range for a node or site.
#[derive(Debug, Clone)]
pub struct EcosafetyTrend {
    pub node_id: String,
    pub n_windows: u64,
    pub k_min: f32,
    pub k_max: f32,
    pub k_mean: f32,
    pub e_min: f32,
    pub e_max: f32,
    pub e_mean: f32,
    pub r_min: f32,
    pub r_max: f32,
    pub r_mean: f32,
    pub vt_max: f32,
    pub roh_max: f32,
}

/// Read‑only index wrapper for ecosafety histories.
pub struct EcosafetyIndex {
    conn: Connection,
}

impl EcosafetyIndex {
    pub fn open_ro(path: &str) -> rusqlite::Result<Self> {
        let conn = Connection::open_with_flags(
            path,
            rusqlite::OpenFlags::SQLITE_OPEN_READ_ONLY,
        )?;
        Ok(Self { conn })
    }

    /// Return full status history for a node between timestamps (inclusive).
    pub fn history_for_node(
        &self,
        node_id: &str,
        start_utc: &str,
        end_utc: &str,
    ) -> rusqlite::Result<Vec<EcosafetyNodeStatus>> {
        let mut stmt = self.conn.prepare(
            "SELECT node_id, window_start_utc, window_end_utc, lane, \
                    k, e, r, vt, roh, ecosafety_state \
             FROM cybo_node_ecosafety_envelope \
             WHERE node_id = ?1 \
               AND window_start_utc >= ?2 \
               AND window_end_utc <= ?3 \
             ORDER BY window_start_utc",
        )?;

        let rows = stmt.query_map(params![node_id, start_utc, end_utc], |row| {
            Ok(EcosafetyNodeStatus {
                node_id: row.get(0)?,
                window_start_utc: row.get(1)?,
                window_end_utc: row.get(2)?,
                lane: row.get(3)?,
                k: row.get(4)?,
                e: row.get(5)?,
                r: row.get(6)?,
                vt: row.get(7)?,
                roh: row.get(8)?,
                ecosafety_state: row.get(9)?,
            })
        })?;

        let mut out = Vec::new();
        for r in rows {
            out.push(r?);
        }
        Ok(out)
    }

    /// Compute simple K/E/R/Vt/RoH trend statistics for a node and time range.
    pub fn trend_for_node(
        &self,
        node_id: &str,
        start_utc: &str,
        end_utc: &str,
    ) -> rusqlite::Result<Option<EcosafetyTrend>> {
        let mut stmt = self.conn.prepare(
            "SELECT COUNT(*) as n,
                    MIN(k), MAX(k), AVG(k),
                    MIN(e), MAX(e), AVG(e),
                    MIN(r), MAX(r), AVG(r),
                    MAX(vt), MAX(roh)
             FROM cybo_node_ecosafety_envelope
             WHERE node_id = ?1
               AND window_start_utc >= ?2
               AND window_end_utc <= ?3",
        )?;

        let mut rows = stmt.query(params![node_id, start_utc, end_utc])?;
        if let Some(row) = rows.next() {
            let row = row?;
            let n: i64 = row.get(0)?;
            if n == 0 {
                return Ok(None);
            }
            Ok(Some(EcosafetyTrend {
                node_id: node_id.to_string(),
                n_windows: n as u64,
                k_min: row.get(1)?,
                k_max: row.get(2)?,
                k_mean: row.get(3)?,
                e_min: row.get(4)?,
                e_max: row.get(5)?,
                e_mean: row.get(6)?,
                r_min: row.get(7)?,
                r_max: row.get(8)?,
                r_mean: row.get(9)?,
                vt_max: row.get(10)?,
                roh_max: row.get(11)?,
            }))
        } else {
            Ok(None)
        }
    }
}
