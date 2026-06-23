// filename: crates/ecocybo_planner/src/lib.rs
// destination: eco_restoration_shard/crates/ecocybo_planner/src/lib.rs

#![forbid(unsafe_code)]

use chrono::{DateTime, Utc};
use rusqlite::{Connection, OpenFlags};
use serde::Serialize;
use std::path::Path;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum PlannerError {
    #[error("SQL error: {0}")]
    Sql(#[from] rusqlite::Error),
    #[error("parse error: {0}")]
    Parse(String),
}

#[derive(Debug, Serialize)]
pub struct NodeRestorationSummary {
    pub node_id: String,
    pub region: String,
    pub node_family: String,
    pub eco_restorative_windows: i64,
    pub total_windows: i64,
    pub frac_restorative: f64,
    pub avg_carbon_delta_kg: f64,
    pub avg_eco_efficiency: f64,
    pub latest_window_end_utc: String,
}

/// Open EcoCybo DB read-only.
fn open_ro(db_path: &str) -> Result<Connection, PlannerError> {
    Ok(Connection::open_with_flags(
        Path::new(db_path),
        OpenFlags::SQLITE_OPEN_READONLY | OpenFlags::SQLITE_OPEN_NOMUTEX,
    )?)
}

/// Compute restoration summaries per node over all recorded windows.
pub fn compute_restoration_summaries(
    db_path: &str,
) -> Result<Vec<NodeRestorationSummary>, PlannerError> {
    let conn = open_ro(db_path)?;

    let mut stmt = conn.prepare(
        r#"
        SELECT
            n.node_id,
            n.region,
            n.node_family,
            SUM(CASE WHEN s.eco_restorative = 1 THEN 1 ELSE 0 END) AS eco_restorative_windows,
            COUNT(*) AS total_windows,
            AVG(s.carbon_delta_kg) AS avg_carbon_delta_kg,
            AVG(s.eco_efficiency)  AS avg_eco_efficiency,
            MAX(s.window_end_utc)  AS latest_window_end_utc
        FROM cybo_node_eco_score s
        JOIN cybo_node n ON n.node_id = s.node_id
        GROUP BY n.node_id, n.region, n.node_family
        ORDER BY frac_restorative DESC
        "#,
    )?;

    // SQLite cannot reference derived alias in ORDER BY inside same SELECT,
    // so compute frac_restorative in Rust and sort there.
    let rows = stmt.query_map([], |row| {
        let node_id: String = row.get(0)?;
        let region: String = row.get(1)?;
        let node_family: String = row.get(2)?;
        let eco_restorative_windows: i64 = row.get(3)?;
        let total_windows: i64 = row.get(4)?;
        let avg_carbon_delta_kg: f64 = row.get(5)?;
        let avg_eco_efficiency: f64 = row.get(6)?;
        let latest_window_end_utc: String = row.get(7)?;

        let frac_restorative = if total_windows > 0 {
            eco_restorative_windows as f64 / total_windows as f64
        } else {
            0.0
        };

        Ok(NodeRestorationSummary {
            node_id,
            region,
            node_family,
            eco_restorative_windows,
            total_windows,
            frac_restorative,
            avg_carbon_delta_kg,
            avg_eco_efficiency,
            latest_window_end_utc,
        })
    })?;

    let mut out: Vec<NodeRestorationSummary> = Vec::new();
    for r in rows {
        out.push(r?);
    }

    // Sort by restorative fraction, then by more negative carbon (better), then by efficiency.
    out.sort_by(|a, b| {
        b.frac_restorative
            .partial_cmp(&a.frac_restorative)
            .unwrap_or(std::cmp::Ordering::Equal)
            .then_with(|| {
                // more negative carbon is better
                a.avg_carbon_delta_kg
                    .partial_cmp(&b.avg_carbon_delta_kg)
                    .unwrap_or(std::cmp::Ordering::Equal)
            })
            .then_with(|| {
                b.avg_eco_efficiency
                    .partial_cmp(&a.avg_eco_efficiency)
                    .unwrap_or(std::cmp::Ordering::Equal)
            })
    });

    Ok(out)
}

/// Filter summaries to those meeting a minimum restorative fraction and carbon negativity.
pub fn filter_high_value_nodes(
    summaries: &[NodeRestorationSummary],
    min_frac_restorative: f64,
    max_avg_carbon_delta_kg: f64,
) -> Vec<NodeRestorationSummary> {
    summaries
        .iter()
        .cloned()
        .filter(|s| {
            s.frac_restorative >= min_frac_restorative
                && s.avg_carbon_delta_kg <= max_avg_carbon_delta_kg
        })
        .collect()
}

/// Simple helper: determine if a node is safe to promote
/// to a higher lane from an eco-restoration standpoint.
/// This is still non-actuating; decisions are just data.
pub fn can_promote_node(summary: &NodeRestorationSummary) -> bool {
    summary.frac_restorative >= 0.8 && summary.avg_carbon_delta_kg <= -10.0
        && summary.avg_eco_efficiency >= 1.0
}
