// Filename: crates/cyboquatic_spine/src/hydraulic_breach_queries.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
// !forbidunsafecode

use std::os::raw::c_char;

use rusqlite::{params, Connection, Row};
use serde::Serialize;

use crate::{cstr_to_str, implffiquery, ShardIndex, SpineError};

#[derive(Debug, Clone, Serialize)]
pub struct HydraulicInstantHit {
    pub node_id: String,
    pub max_surcharge_pa: f64,
}

#[derive(Debug, Clone, Serialize)]
pub struct HydraulicDailyHit {
    pub node_id: String,
    pub max_surcharge_pa: f64,
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub vt: f64,
}

#[derive(Debug, Clone)]
pub struct HydraulicBreachParams {
    pub lat_b: f64,
    pub lon_b: f64,
    pub dlat: f64,
    pub dlon: f64,
    pub ts_window_start: String,
    pub ts_window_end: String,
    pub x_pa: f64,
    pub breach_day: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct HydraulicBreachBundle {
    pub instant_hits: Vec<HydraulicInstantHit>,
    pub daily_hits: Vec<HydraulicDailyHit>,
}

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

    let mut stmt = conn.prepare(sql_spatial_candidates).map_err(SpineError::from)?;

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

    let mut stmt = conn.prepare(sql_daily).map_err(SpineError::from)?;

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

implffiquery! {
    #[no_mangle]
    pub extern "C" fn cyboquatic_get_hydraulic_breach_bundle(
        handle: *mut ShardIndex,
        breach_lat: *const c_char,
        breach_lon: *const c_char,
        dlat: *const c_char,
        dlon: *const c_char,
        ts_window_start: *const c_char,
        ts_window_end: *const c_char,
        x_pa: *const c_char,
        breach_day: *const c_char,
    ) -> *mut c_char {
        if handle.is_null() {
            return Err(SpineError::InvalidArgument(
                "Null ShardIndex handle passed to cyboquatic_get_hydraulic_breach_bundle".to_string(),
            ));
        }

        let lat_str = cstr_to_str(breach_lat)?;
        let lon_str = cstr_to_str(breach_lon)?;
        let dlat_str = cstr_to_str(dlat)?;
        let dlon_str = cstr_to_str(dlon)?;
        let ts_start_str = cstr_to_str(ts_window_start)?;
        let ts_end_str = cstr_to_str(ts_window_end)?;
        let x_pa_str = cstr_to_str(x_pa)?;
        let breach_day_str = cstr_to_str(breach_day)?;

        if lat_str.is_empty()
            || lon_str.is_empty()
            || dlat_str.is_empty()
            || dlon_str.is_empty()
            || ts_start_str.is_empty()
            || ts_end_str.is_empty()
            || x_pa_str.is_empty()
            || breach_day_str.is_empty()
        {
            return Err(SpineError::InvalidArgument(
                "All hydraulic breach parameters must be non-empty strings".to_string(),
            ));
        }

        let lat_b: f64 = lat_str.parse().map_err(|e| {
            SpineError::InvalidArgument(format!("Invalid breach_lat '{}': {}", lat_str, e))
        })?;
        let lon_b: f64 = lon_str.parse().map_err(|e| {
            SpineError::InvalidArgument(format!("Invalid breach_lon '{}': {}", lon_str, e))
        })?;
        let dlat_val: f64 = dlat_str.parse().map_err(|e| {
            SpineError::InvalidArgument(format!("Invalid dlat '{}': {}", dlat_str, e))
        })?;
        let dlon_val: f64 = dlon_str.parse().map_err(|e| {
            SpineError::InvalidArgument(format!("Invalid dlon '{}': {}", dlon_str, e))
        })?;
        let x_pa_val: f64 = x_pa_str.parse().map_err(|e| {
            SpineError::InvalidArgument(format!("Invalid X_pa '{}': {}", x_pa_str, e))
        })?;

        let params = HydraulicBreachParams {
            lat_b,
            lon_b,
            dlat: dlat_val,
            dlon: dlon_val,
            ts_window_start: ts_start_str.to_string(),
            ts_window_end: ts_end_str.to_string(),
            x_pa: x_pa_val,
            breach_day: breach_day_str.to_string(),
        };

        let shard_index = unsafe { &mut *handle };
        let bundle = query_hydraulic_breach_bundle(&shard_index.conn, &params)?;
        Ok(bundle)
    }
}
