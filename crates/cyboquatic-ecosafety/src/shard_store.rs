// filename: cyboquatic-ecosafety/src/shard_store.rs
// destination: cyboquatic-ecosafety/src/shard_store.rs

#![forbid(unsafe_code)]

#[cfg(feature = "sqlite-storage")]
use rusqlite::{params, Connection};

use alloc::string::String;
use crate::core::ShardUpdate;

/// Phoenix ecosafety row shape (must match migration schema exactly).
#[derive(Debug, Clone)]
pub struct CyboNodeEcosafetyEnvelope {
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
    pub evidence_hex: String,
    pub signing_hex: String,
}

/// Read‑only handle for ecosafety shard store; mutation only via controlled paths.
#[cfg(feature = "sqlite-storage")]
pub struct ShardStore {
    conn: Connection,
}

#[cfg(feature = "sqlite-storage")]
impl ShardStore {
    /// Open an existing SQLite file in read‑write mode for controlled writers.
    /// Callers outside migration and ingestion tools should only use read helpers.
    pub fn open_rw(path: &str) -> rusqlite::Result<Self> {
        let conn = Connection::open(path)?;
        Ok(Self { conn })
    }

    /// Open an existing SQLite file in read‑only mode.
    pub fn open_ro(path: &str) -> rusqlite::Result<Self> {
        let conn = Connection::open_with_flags(
            path,
            rusqlite::OpenFlags::SQLITE_OPEN_READ_ONLY,
        )?;
        Ok(Self { conn })
    }

    /// Insert a validated ecosafety shard.
    /// This should only be called by the pipeline that already ran IntegrityCheckFrame.
    pub fn insert_shard(&self, shard: &CyboNodeEcosafetyEnvelope) -> rusqlite::Result<()> {
        self.conn.execute(
            "INSERT INTO cybo_node_ecosafety_envelope \
             (node_id, window_start_utc, window_end_utc, lane, \
              k, e, r, vt, roh, ecosafety_state, evidence_hex, signing_hex) \
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)",
            params![
                shard.node_id,
                shard.window_start_utc,
                shard.window_end_utc,
                shard.lane,
                shard.k,
                shard.e,
                shard.r,
                shard.vt,
                shard.roh,
                shard.ecosafety_state,
                shard.evidence_hex,
                shard.signing_hex,
            ],
        )?;
        Ok(())
    }

    /// Read‑only query; returns all envelopes for a node.
    pub fn list_by_node(&self, node_id: &str) -> rusqlite::Result<Vec<CyboNodeEcosafetyEnvelope>> {
        let mut stmt = self.conn.prepare(
            "SELECT node_id, window_start_utc, window_end_utc, lane, \
                    k, e, r, vt, roh, ecosafety_state, evidence_hex, signing_hex \
             FROM cybo_node_ecosafety_envelope \
             WHERE node_id = ?1 \
             ORDER BY window_start_utc",
        )?;

        let rows = stmt.query_map([node_id], |row| {
            Ok(CyboNodeEcosafetyEnvelope {
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
                evidence_hex: row.get(10)?,
                signing_hex: row.get(11)?,
            })
        })?;

        let mut out = Vec::new();
        for r in rows {
            out.push(r?);
        }
        Ok(out)
    }
}
