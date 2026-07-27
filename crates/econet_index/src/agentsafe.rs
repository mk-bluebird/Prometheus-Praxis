// filename: crates/econet_index/src/agentsafe.rs

use serde::Serialize;
use rusqlite::{Connection, Row};
use crate::SpineError;

#[derive(Serialize)]
pub struct AgentSafeEntry {
    pub objectid: String,
    pub kind: String,
    pub roleband: String,
    pub lanescope: String,
    pub blastradius_class: String,
    pub aicapabilitylevel: String,
    pub reponame: String,
    pub path_or_handle: String,
    pub summary: String,
    pub versiontag: String,
    pub status: String,
}

fn map_row(row: &Row<'_>) -> rusqlite::Result<AgentSafeEntry> {
    Ok(AgentSafeEntry {
        objectid: row.get(0)?,
        kind: row.get(1)?,
        roleband: row.get(2)?,
        lanescope: row.get(3)?,
        blastradius_class: row.get(4)?,
        aicapabilitylevel: row.get(5)?,
        reponame: row.get(6)?,
        path_or_handle: row.get(7)?,
        summary: row.get(8)?,
        versiontag: row.get(9)?,
        status: row.get(10)?,
    })
}

pub fn query_agentsafecatalog_for_repo(
    conn: &Connection,
    reponame: &str,
) -> Result<Vec<AgentSafeEntry>, SpineError> {
    let mut stmt = conn
        .prepare(
            r#"
            SELECT
                objectid,
                kind,
                roleband,
                lanescope,
                blastradius_class,
                aicapabilitylevel,
                reponame,
                path_or_handle,
                summary,
                versiontag,
                status
            FROM vagentsafecatalog
            WHERE reponame = ?1
            ORDER BY objectid;
            "#,
        )
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let rows = stmt
        .query_map([reponame], map_row)
        .map_err(|e| SpineError::Query(e.to_string()))?;

    let mut out = Vec::new();
    for row in rows {
        out.push(row.map_err(|e| SpineError::Query(e.to_string()))?);
    }
    Ok(out)
}
