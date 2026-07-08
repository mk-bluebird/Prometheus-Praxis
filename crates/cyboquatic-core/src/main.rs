// crates/cyboquatic_lane_replay/src/main.rs
// Non‑actuating lane promotion replay harness for Cyboquatic nodes in Phoenix.
// Uses the same KER and residual semantics as the spine views to prove
// non-regression of lane upgrades (RESEARCH -> EXPPROD/PROD).[file:3][file:6]

#![forbid(unsafe_code)]

use std::env;
use std::path::PathBuf;

use rusqlite::{params, Connection};
use thiserror::Error;

#[derive(Debug, Error)]
enum ReplayError {
    #[error("SQLite error: {0}")]
    SQLite(#[from] rusqlite::Error),

    #[error("missing shard '{0}' in snapshot")]
    MissingShard(String),
}

#[derive(Debug, Clone)]
struct KerSnapshot {
    shardid: String,
    kerk: f64,
    kere: f64,
    kerr: f64,
    vt_with_topology: f64,
}

fn load_ker_snapshot(conn: &Connection) -> Result<Vec<KerSnapshot>, ReplayError> {
    // vshardker is the canonical KER view with topology overlays.[file:3]
    let mut stmt = conn.prepare(
        "SELECT shardid, kerk, kere, kerr, vtwithtopology \
         FROM vshardker",
    )?;
    let mut rows = stmt.query([])?;
    let mut result = Vec::new();
    while let Some(row) = rows.next()? {
        result.push(KerSnapshot {
            shardid: row.get(0)?,
            kerk: row.get(1)?,
            kere: row.get(2)?,
            kerr: row.get(3)?,
            vt_with_topology: row.get(4)?,
        });
    }
    Ok(result)
}

fn main() -> Result<(), ReplayError> {
    let mut args = env::args().skip(1);
    let snapshot_a_path = args
        .next()
        .expect("expected first argument: path to snapshot A DB");
    let snapshot_b_path = args
        .next()
        .expect("expected second argument: path to snapshot B DB");

    let eps_k: f64 = env::var("CYBO_EPS_K")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(1e-6);
    let eps_e: f64 = env::var("CYBO_EPS_E")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(1e-6);
    let eps_r: f64 = env::var("CYBO_EPS_R")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(1e-6);
    let eps_v: f64 = env::var("CYBO_EPS_V")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(1e-6);

    let conn_a = Connection::open(PathBuf::from(snapshot_a_path))?;
    let conn_b = Connection::open(PathBuf::from(snapshot_b_path))?;

    let a = load_ker_snapshot(&conn_a)?;
    let b = load_ker_snapshot(&conn_b)?;

    let mut violations: Vec<String> = Vec::new();

    for a_row in &a {
        let shard_id = &a_row.shardid;
        let b_row_opt = b.iter().find(|r| &r.shardid == shard_id);
        let b_row = match b_row_opt {
            Some(r) => r,
            None => {
                violations.push(format!(
                    "shard {} missing in snapshot B",
                    shard_id
                ));
                continue;
            }
        };

        if b_row.kerk + eps_k < a_row.kerk {
            violations.push(format!(
                "K regression for shard {}: new {:.6} < old {:.6}",
                shard_id, b_row.kerk, a_row.kerk
            ));
        }
        if b_row.kere + eps_e < a_row.kere {
            violations.push(format!(
                "E regression for shard {}: new {:.6} < old {:.6}",
                shard_id, b_row.kere, a_row.kere
            ));
        }
        if b_row.kerr > a_row.kerr + eps_r {
            violations.push(format!(
                "R regression for shard {}: new {:.6} > old {:.6}",
                shard_id, b_row.kerr, a_row.kerr
            ));
        }
        if b_row.vt_with_topology > a_row.vt_with_topology + eps_v {
            violations.push(format!(
                "Vt regression for shard {}: new {:.6} > old {:.6}",
                shard_id, b_row.vt_with_topology, a_row.vt_with_topology
            ));
        }
    }

    // Record violations into an append-only table suitable for CI and audit.
    if !violations.is_empty() {
        let conn_out = Connection::open("cyboquatic_lane_replay_violations.db")?;
        conn_out.execute_batch(
            "PRAGMA foreign_keys = ON;
             CREATE TABLE IF NOT EXISTS LanePromotionReplayViolation2026v1 (
                 id         INTEGER PRIMARY KEY AUTOINCREMENT,
                 shardid    TEXT    NOT NULL,
                 message    TEXT    NOT NULL,
                 created_utc INTEGER NOT NULL DEFAULT (strftime('%s','now'))
             );",
        )?;

        let tx = conn_out.transaction()?;
        {
            let mut insert = tx.prepare(
                "INSERT INTO LanePromotionReplayViolation2026v1 (shardid, message)
                 VALUES (?1, ?2)",
            )?;
            for msg in &violations {
                // Extract shardid from message prefix if present.
                let shardid = msg
                    .split_whitespace()
                    .nth(3)
                    .unwrap_or("unknown")
                    .to_string();
                insert.execute(params![shardid, msg])?;
            }
        }
        tx.commit()?;

        eprintln!("Lane promotion replay found violations:");
        for v in violations {
            eprintln!("  - {}", v);
        }
        std::process::exit(1);
    } else {
        println!("Lane promotion replay passed with no violations.");
    }

    Ok(())
}
