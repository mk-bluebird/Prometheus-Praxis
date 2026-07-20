// Filename: crates/cyboquatic_spine/src/hydraulic_breach_queries.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
// !forbidunsafecode

use rusqlite::{params, Connection, Row};
use serde::Serialize;

use crate::SpineError;

/// Instant hydraulic breach hit for a node in a given time window.
#[derive(Debug, Clone, Serialize)]
pub struct HydraulicInstantHit {
    pub node_id: String,
    pub max_surcharge_pa: f64,
}

/// Daily surcharge diagnostics around a breach day for a given node.
#[derive(Debug, Clone, Serialize)]
pub struct HydraulicDailyHit {
    pub node_id: String,
    pub max_surcharge_pa: f64,
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub vt: f64,
}

/// Input parameters describing a spatial breach window and thresholds.
#[derive(Debug, Clone)]
pub struct HydraulicBreachParams {
    pub lat_b: f64,
    pub lon_b: f64,
    pub dlat: f64,
    pub dlon: f64,
    pub ts_window_start: String, // ISO-8601 UTC
    pub ts_window_end: String,   // ISO-8601 UTC
    pub x_pa: f64,               // surcharge threshold in Pascals
    pub breach_day: String,      // YYYY-MM-DD UTC day
}

/// Query function: nodes with surcharge > X_pa in the given time window,
/// restricted to an rtree_canal_node spatial envelope around (lat_b, lon_b).
pub fn query_hydraulic_instant_hits(
    conn: &Connection,
    params_in: &HydraulicBreachParams,
) -> Result<Vec<HydraulicInstantHit>, SpineError> {
    let sql_spatial_candidates = r#"
        WITH spatial_candidates AS (
          SELECT c.node_id
          FROM rtree_canal_node AS r
          JOIN canal_node AS c ON c.rowid = r.node_rowid
          WHERE r.min_lat <= :lat_b + :dlat
            AND r.max_lat >= :lat_b - :dlat
            AND r.min_lon <= :lon_b + :dlon
            AND r.max_lon >= :lon_b - :dlon
        ),
        hydraulic_hits AS (
          SELECT s.node_id, MAX(s.surcharge_pa) AS max_surcharge_pa
          FROM node_surcharge AS s
          JOIN spatial_candidates AS sc ON sc.node_id = s.node_id
          WHERE s.ts_utc BETWEEN :ts_window_start AND :ts_window_end
          GROUP BY s.node_id
        )
        SELECT h.node_id, h.max_surcharge_pa
        FROM hydraulic_hits AS h
        WHERE h.max_surcharge_pa > :X_pa
    "#;

    let mut stmt = conn
        .prepare(sql_spatial_candidates)
        .map_err(SpineError::from)?;

    let rows = stmt
        .query_map(
            params![
                params_in.lat_b,
                params_in.lon_b,
                params_in.dlat,
                params_in.dlon,
                params_in.ts_window_start,
                params_in.ts_window_end,
                params_in.x_pa
            ],
            map_instant_hit_row,
        )
        .map_err(SpineError::from)?;

    let mut out = Vec::new();
    for row_res in rows {
        let hit = row_res.map_err(SpineError::from)?;
        out.push(hit);
    }

    Ok(out)
}

/// Query function: daily surcharge KER/Vt diagnostics for spatial envelope
/// on a single breach day, thresholded by X_pa.
pub fn query_hydraulic_daily_hits(
    conn: &Connection,
    params_in: &HydraulicBreachParams,
) -> Result<Vec<HydraulicDailyHit>, SpineError> {
    let sql_daily = r#"
        WITH spatial_candidates AS (
          SELECT c.node_id
          FROM rtree_canal_node AS r
          JOIN canal_node AS c ON c.rowid = r.node_rowid
          WHERE r.min_lat <= :lat_b + :dlat
            AND r.max_lat >= :lat_b - :dlat
            AND r.min_lon <= :lon_b + :dlon
            AND r.max_lon >= :lon_b - :dlon
        )
        SELECT d.node_id, d.max_surcharge_pa, d.K, d.E, d.R, d.Vt
        FROM daily_surcharge AS d
        JOIN spatial_candidates AS sc ON sc.node_id = d.node_id
        WHERE d.day_utc = :breach_day
          AND d.max_surcharge_pa > :X_pa
    "#;

    let mut stmt = conn
        .prepare(sql_daily)
        .map_err(SpineError::from)?;

    let rows = stmt
        .query_map(
            params![
                params_in.lat_b,
                params_in.lon_b,
                params_in.dlat,
                params_in.dlon,
                params_in.breach_day,
                params_in.x_pa
            ],
            map_daily_hit_row,
        )
        .map_err(SpineError::from)?;

    let mut out = Vec::new();
    for row_res in rows {
        let hit = row_res.map_err(SpineError::from)?;
        out.push(hit);
    }

    Ok(out)
}

/// Convenience helper: run both instant and daily queries in one call.
#[derive(Debug, Clone, Serialize)]
pub struct HydraulicBreachBundle {
    pub instant_hits: Vec<HydraulicInstantHit>,
    pub daily_hits: Vec<HydraulicDailyHit>,
}

pub fn query_hydraulic_breach_bundle(
    conn: &Connection,
    params_in: &HydraulicBreachParams,
) -> Result<HydraulicBreachBundle, SpineError> {
    let instant = query_hydraulic_instant_hits(conn, params_in)?;
    let daily = query_hydraulic_daily_hits(conn, params_in)?;
    Ok(HydraulicBreachBundle {
        instant_hits: instant,
        daily_hits: daily,
    })
}

fn map_instant_hit_row(row: &Row) -> rusqlite::Result<HydraulicInstantHit> {
    Ok(HydraulicInstantHit {
        node_id: row.get(0)?,
        max_surcharge_pa: row.get(1)?,
    })
}

fn map_daily_hit_row(row: &Row) -> rusqlite::Result<HydraulicDailyHit> {
    Ok(HydraulicDailyHit {
        node_id: row.get(0)?,
        max_surcharge_pa: row.get(1)?,
        k: row.get(2)?,
        e: row.get(3)?,
        r: row.get(4)?,
        vt: row.get(5)?,
    })
}
