// filename: src/econet_cyboquatic_index.rs
// destination: eco_restoration_shard/src/econet_cyboquatic_index.rs
// Purpose:
// - Rust helper module for the Cyboquatic machinery SQLite spine.
// - Non-actuating, read-only queries that expose JSON-ready structs for Lua / Kotlin.
// - Provides "always-improve" oriented summaries for K, E, R and eco-restorative candidates.

#![forbid(unsafe_code)]

use serde::Serialize;
use rusqlite::{Connection, OpenFlags, Result as SqlResult};

#[derive(Debug, Serialize)]
pub struct CyboNodeScore {
    pub nodeid: String,
    pub region: String,
    pub medium: String,
    pub energy_req_j: f64,
    pub energy_surplus_j: f64,
    pub carbon_kg: f64,
    pub offset_kg: f64,
    pub is_carbon_negative: bool,
    pub avg_delta_vt: f64,
    pub avg_rplane: f64,
    pub is_ecorestorative_candidate: bool,
}

#[derive(Debug, Serialize)]
pub struct CyboKerEnvelope {
    pub scopetype: String,
    pub scoperefid: String,
    pub kfactor: f64,
    pub efactor: f64,
    pub rfactor: f64,
    pub evaluation_utc: String,
    pub evaluator_did: String,
    pub rationale: String,
}

fn open_readonly(db_path: &str) -> SqlResult<Connection> {
    Connection::open_with_flags(
        db_path,
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
    )
}

pub fn list_ecorestorative_nodes(db_path: &str, rplane_max: f64) -> SqlResult<Vec<CyboNodeScore>> {
    let conn = open_readonly(db_path)?;
    let mut stmt = conn.prepare(
        r#"
        SELECT
            nodeid,
            region,
            medium,
            energy_req_j,
            energy_surplus_j,
            carbon_kg,
            offset_kg,
            is_carbon_negative,
            avg_delta_vt,
            avg_rplane,
            is_ecorestorative_candidate
        FROM v_cybo_ecorestorative_score
        WHERE avg_rplane <= ?1
        ORDER BY is_ecorestorative_candidate DESC, avg_rplane ASC
        "#,
    )?;

    let rows = stmt.query_map([rplane_max], |row| {
        Ok(CyboNodeScore {
            nodeid: row.get(0)?,
            region: row.get(1)?,
            medium: row.get(2)?,
            energy_req_j: row.get(3)?,
            energy_surplus_j: row.get(4)?,
            carbon_kg: row.get(5)?,
            offset_kg: row.get(6)?,
            is_carbon_negative: row.get::<_, i64>(7)? == 1,
            avg_delta_vt: row.get(8)?,
            avg_rplane: row.get(9)?,
            is_ecorestorative_candidate: row.get::<_, i64>(10)? == 1,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

pub fn latest_ker_scores_for_scope(
    db_path: &str,
    scopetype: &str,
    scoperefid: &str,
) -> SqlResult<Vec<CyboKerEnvelope>> {
    let conn = open_readonly(db_path)?;
    let mut stmt = conn.prepare(
        r#"
        SELECT
            scopetype,
            scoperefid,
            kfactor,
            efactor,
            rfactor,
            evaluation_utc,
            evaluator_did,
            COALESCE(rationale, '')
        FROM cybo_ker_scores
        WHERE scopetype = ?1 AND scoperefid = ?2
        ORDER BY evaluation_utc DESC
        LIMIT 10
        "#,
    )?;

    let rows = stmt.query_map([scopetype, scoperefid], |row| {
        Ok(CyboKerEnvelope {
            scopetype: row.get(0)?,
            scoperefid: row.get(1)?,
            kfactor: row.get(2)?,
            efactor: row.get(3)?,
            rfactor: row.get(4)?,
            evaluation_utc: row.get(5)?,
            evaluator_did: row.get(6)?,
            rationale: row.get(7)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}
