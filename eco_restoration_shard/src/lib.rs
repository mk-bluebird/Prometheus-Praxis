// filename: eco_restoration_shard/src/lib.rs
// destination: eco_restoration_shard/src/lib.rs
// crate-type: cdylib (configure in Cargo.toml)
// Rust edition: 2024, rust-version: 1.85

#![forbid(unsafe_code)]

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use std::path::Path;

use rusqlite::{Connection, OpenFlags};
use serde::Serialize;

#[derive(Debug, Serialize)]
pub struct NodeEcoScore {
    pub node_id: String,
    pub window_start_utc: String,
    pub window_end_utc: String,
    pub k_avg: f64,
    pub e_avg: f64,
    pub r_avg: f64,
    pub vt_max: f64,
    pub eco_gain: f64,
    pub energy_cost_kwh: f64,
    pub carbon_delta_kg: f64,
    pub materials_residue: f64,
    pub biodiv_delta: f64,
    pub eco_efficiency: f64,
    pub reward_class: String,
    pub eco_restorative: bool,
}

#[derive(Debug, Serialize)]
pub struct NodeBlastRadius {
    pub node_id: String,
    pub impact_plane: String,
    pub avg_impact_score: f64,
    pub max_impact_score: f64,
}

#[derive(Debug, Serialize)]
pub struct NodeWorkloadTrend {
    pub node_id: String,
    pub channel: String,
    pub total_request_j: f64,
    pub total_surplus_j: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_r_energy: Option<f64>,
    pub mean_r_carbon: Option<f64>,
    pub mean_r_materials: Option<f64>,
    pub mean_r_biodiv: Option<f64>,
    pub mean_r_hydraulics: Option<f64>,
}

/// Open SQLite DB read-only, no mutex (cdylib-style access).
fn open_ro_db(db_path: &str) -> rusqlite::Result<Connection> {
    Connection::open_with_flags(
        Path::new(db_path),
        OpenFlags::SQLITE_OPEN_READONLY | OpenFlags::SQLITE_OPEN_NOMUTEX,
    )
}

/// Convert a C string to Rust &str with basic safety.
unsafe fn cstr_to_str<'a>(ptr: *const c_char) -> Result<&'a str, &'static str> {
    if ptr.is_null() {
        return Err("null pointer");
    }
    CStr::from_ptr(ptr).to_str().map_err(|_| "invalid UTF-8")
}

/// Serialize any Serialize value to a newly allocated C string.
/// Caller must free via econet_free_json.
fn to_json_cstring<T: Serialize>(val: &T) -> *mut c_char {
    match serde_json::to_string(val) {
        Ok(json) => match CString::new(json) {
            Ok(cstr) => cstr.into_raw(),
            Err(_) => ptr::null_mut(),
        },
        Err(_) => ptr::null_mut(),
    }
}

/// Wrap error message into JSON: { "error": "<msg>" }.
fn error_json(msg: &str) -> *mut c_char {
    #[derive(Serialize)]
    struct Error<'a> {
        error: &'a str,
    }
    to_json_cstring(&Error { error: msg })
}

/// Query eco scores for a node_id, ordered by window_end_utc descending.
fn query_node_eco_scores(
    conn: &Connection,
    node_id: &str,
) -> rusqlite::Result<Vec<NodeEcoScore>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            node_id,
            window_start_utc,
            window_end_utc,
            k_avg,
            e_avg,
            r_avg,
            vt_max,
            eco_gain,
            energy_cost_kwh,
            carbon_delta_kg,
            materials_residue,
            biodiv_delta,
            eco_efficiency,
            reward_class,
            eco_restorative
        FROM cybo_node_eco_score
        WHERE node_id = ?1
        ORDER BY window_end_utc DESC
        "#,
    )?;

    let rows = stmt.query_map([node_id], |row| {
        Ok(NodeEcoScore {
            node_id: row.get(0)?,
            window_start_utc: row.get(1)?,
            window_end_utc: row.get(2)?,
            k_avg: row.get(3)?,
            e_avg: row.get(4)?,
            r_avg: row.get(5)?,
            vt_max: row.get(6)?,
            eco_gain: row.get(7)?,
            energy_cost_kwh: row.get(8)?,
            carbon_delta_kg: row.get(9)?,
            materials_residue: row.get(10)?,
            biodiv_delta: row.get(11)?,
            eco_efficiency: row.get(12)?,
            reward_class: row.get(13)?,
            eco_restorative: {
                let v: i64 = row.get(14)?;
                v != 0
            },
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

/// Query blast-radius aggregates per plane for a node_id.
fn query_node_blast_radius(
    conn: &Connection,
    node_id: &str,
) -> rusqlite::Result<Vec<NodeBlastRadius>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            node_id,
            impact_plane,
            avg_impact_score,
            max_impact_score
        FROM v_cybo_node_blast_aggregate
        WHERE node_id = ?1
        ORDER BY impact_plane
        "#,
    )?;

    let rows = stmt.query_map([node_id], |row| {
        Ok(NodeBlastRadius {
            node_id: row.get(0)?,
            impact_plane: row.get(1)?,
            avg_impact_score: row.get(2)?,
            max_impact_score: row.get(3)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

/// Query workload trends per channel for a node_id.
fn query_node_workload_trends(
    conn: &Connection,
    node_id: &str,
) -> rusqlite::Result<Vec<NodeWorkloadTrend>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            node_id,
            channel,
            SUM(e_req_j)      AS total_request_j,
            SUM(e_surplus_j)  AS total_surplus_j,
            AVG(vt_before)    AS mean_vt_before,
            AVG(vt_after)     AS mean_vt_after,
            AVG(r_energy)     AS mean_r_energy,
            AVG(r_carbon)     AS mean_r_carbon,
            AVG(r_materials)  AS mean_r_materials,
            AVG(r_biodiv)     AS mean_r_biodiv,
            AVG(r_hydraulics) AS mean_r_hydraulics
        FROM cybo_node_workload
        WHERE node_id = ?1
        GROUP BY node_id, channel
        ORDER BY channel
        "#,
    )?;

    let rows = stmt.query_map([node_id], |row| {
        Ok(NodeWorkloadTrend {
            node_id: row.get(0)?,
            channel: row.get(1)?,
            total_request_j: row.get(2)?,
            total_surplus_j: row.get(3)?,
            mean_vt_before: row.get(4)?,
            mean_vt_after: row.get(5)?,
            mean_r_energy: row.get(6)?,
            mean_r_carbon: row.get(7)?,
            mean_r_materials: row.get(8)?,
            mean_r_biodiv: row.get(9)?,
            mean_r_hydraulics: row.get(10)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_node_eco_scores(
    db_path: *const c_char,
    node_id: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(db_path) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let node = match cstr_to_str(node_id) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };

    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite DB"),
    };

    match query_node_eco_scores(&conn, node) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_node_blast_radius(
    db_path: *const c_char,
    node_id: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(db_path) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let node = match cstr_to_str(node_id) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };

    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite DB"),
    };

    match query_node_blast_radius(&conn, node) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_get_node_workload_trends(
    db_path: *const c_char,
    node_id: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(db_path) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let node = match cstr_to_str(node_id) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };

    let conn = match open_ro_db(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite DB"),
    };

    match query_node_workload_trends(&conn, node) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("query failed"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn econet_free_json(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    drop(CString::from_raw(ptr));
}
