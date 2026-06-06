// filename: crates/cyboquatic_spine/src/lib.rs
// destination: eco_restoration_shard/crates/cyboquatic_spine/src/lib.rs
// Rust edition: 2024
// Purpose:
//   - Non-actuating cdylib that opens the Cyboquatic eco spine SQLite DB read-only.
//   - Exposes JSON-returning C ABI functions for KER targets, blast-radius overlays,
//     workload windows, and biodegradable substrate summaries.

#![allow(clippy::missing_safety_doc)]

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

use rusqlite::{Connection, OpenFlags};
use serde::Serialize;

#[derive(Debug, Serialize)]
pub struct KerTarget {
    pub node_id: String,
    pub k_metric: f64,
    pub e_metric: f64,
    pub r_metric: f64,
    pub vt_max: f64,
    pub ker_deployable: bool,
}

#[derive(Debug, Serialize)]
pub struct BlastRadiusEntry {
    pub source_type: String,
    pub source_id: String,
    pub target_type: String,
    pub target_id: String,
    pub impact_plane: String,
    pub impact_score_sum: f64,
    pub vt_sensitivity_mean: f64,
    pub link_count: i64,
}

#[derive(Debug, Serialize)]
pub struct WorkloadWindowEntry {
    pub node_id: String,
    pub channel: String,
    pub window_start_utc: String,
    pub window_end_utc: String,
    pub total_requests_j: f64,
    pub total_surplus_j: f64,
    pub accepted_requests_j: f64,
    pub rejected_requests_j: f64,
    pub rerouted_requests_j: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_delta_vt: f64,
    pub mean_r_carbon: Option<f64>,
    pub mean_r_biodiv: Option<f64>,
    pub accept_fraction: Option<f64>,
}

#[derive(Debug, Serialize)]
pub struct SubstrateSummaryEntry {
    pub node_id: String,
    pub material_id: String,
    pub first_start_utc: String,
    pub last_end_utc: String,
    pub mean_k: f64,
    pub mean_e: f64,
    pub mean_r: f64,
    pub vt_min: f64,
    pub vt_max: f64,
    pub deployable_count: i64,
    pub window_count: i64,
}

// Open read-only connection to SQLite DB.
fn open_ro_db(db_path: &str) -> rusqlite::Result<Connection> {
    Connection::open_with_flags(
        Path::new(db_path),
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
    )
}

// Convert C string pointer to Rust &str safely.
unsafe fn cstr_to_str(ptr: *const c_char) -> Result<&'static str, &'static str> {
    if ptr.is_null() {
        return Err("null pointer");
    }
    CStr::from_ptr(ptr)
        .to_str()
        .map_err(|_| "invalid UTF-8")
}

// Query KER-like substrate window for a single node (best window by highest K, lowest R).
fn query_ker_for_node(conn: &Connection, node_id: &str) -> rusqlite::Result<KerTarget> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            node_id,
            AVG(k_metric) AS k_metric,
            AVG(e_metric) AS e_metric,
            AVG(r_metric) AS r_metric,
            MAX(vt_max)   AS vt_max,
            MAX(ker_deployable) AS ker_deployable
        FROM cybo_substrate_window
        WHERE node_id = ?1
        GROUP BY node_id
        LIMIT 1
        "#,
    )?;

    stmt.query_row([node_id], |row| {
        let ker_deployable_i: i64 = row.get(5)?;
        Ok(KerTarget {
            node_id: row.get(0)?,
            k_metric: row.get(1)?,
            e_metric: row.get(2)?,
            r_metric: row.get(3)?,
            vt_max: row.get(4)?,
            ker_deployable: ker_deployable_i != 0,
        })
    })
}

// Query blast-radius summary for a node from v_cybo_node_blastradius.
fn query_blastradius_for_node(
    conn: &Connection,
    node_id: &str,
) -> rusqlite::Result<Vec<BlastRadiusEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            source_type,
            source_id,
            target_type,
            target_id,
            impact_plane,
            impact_score_sum,
            vt_sensitivity_mean,
            link_count
        FROM v_cybo_node_blastradius
        WHERE target_type = 'NODE' AND target_id = ?1
        ORDER BY impact_plane, source_type, source_id
        "#,
    )?;

    let rows = stmt.query_map([node_id], |row| {
        Ok(BlastRadiusEntry {
            source_type: row.get(0)?,
            source_id: row.get(1)?,
            target_type: row.get(2)?,
            target_id: row.get(3)?,
            impact_plane: row.get(4)?,
            impact_score_sum: row.get(5)?,
            vt_sensitivity_mean: row.get(6)?,
            link_count: row.get(7)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

// Query workload windows for a node from v_cybo_workload_window.
fn query_workload_windows_for_node(
    conn: &Connection,
    node_id: &str,
) -> rusqlite::Result<Vec<WorkloadWindowEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            node_id,
            channel,
            window_start_utc,
            window_end_utc,
            total_requests_j,
            total_surplus_j,
            accepted_requests_j,
            rejected_requests_j,
            rerouted_requests_j,
            mean_vt_before,
            mean_vt_after,
            mean_delta_vt,
            mean_r_carbon,
            mean_r_biodiv,
            accept_fraction
        FROM v_cybo_workload_window
        WHERE node_id = ?1
        ORDER BY channel, window_start_utc
        "#,
    )?;

    let rows = stmt.query_map([node_id], |row| {
        Ok(WorkloadWindowEntry {
            node_id: row.get(0)?,
            channel: row.get(1)?,
            window_start_utc: row.get(2)?,
            window_end_utc: row.get(3)?,
            total_requests_j: row.get(4)?,
            total_surplus_j: row.get(5)?,
            accepted_requests_j: row.get(6)?,
            rejected_requests_j: row.get(7)?,
            rerouted_requests_j: row.get(8)?,
            mean_vt_before: row.get(9)?,
            mean_vt_after: row.get(10)?,
            mean_delta_vt: row.get(11)?,
            mean_r_carbon: row.get(12).ok(),
            mean_r_biodiv: row.get(13).ok(),
            accept_fraction: row.get(14).ok(),
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

// Query biodegradable substrate summary for all materials at a node.
fn query_substrate_summary_for_node(
    conn: &Connection,
    node_id: &str,
) -> rusqlite::Result<Vec<SubstrateSummaryEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            node_id,
            material_id,
            first_start_utc,
            last_end_utc,
            mean_k,
            mean_e,
            mean_r,
            vt_min,
            vt_max,
            deployable_count,
            window_count
        FROM v_cybo_substrate_summary
        WHERE node_id = ?1
        ORDER BY material_id
        "#,
    )?;

    let rows = stmt.query_map([node_id], |row| {
        Ok(SubstrateSummaryEntry {
            node_id: row.get(0)?,
            material_id: row.get(1)?,
            first_start_utc: row.get(2)?,
            last_end_utc: row.get(3)?,
            mean_k: row.get(4)?,
            mean_e: row.get(5)?,
            mean_r: row.get(6)?,
            vt_min: row.get(7)?,
            vt_max: row.get(8)?,
            deployable_count: row.get(9)?,
            window_count: row.get(10)?,
        })
    })?;

    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

// Serialize a value to JSON and hand it out as a C string pointer.
fn to_json_c_string<T: Serialize>(val: &T) -> *mut c_char {
    match serde_json::to_string(val) {
        Ok(json) => match CString::new(json) {
            Ok(s) => s.into_raw(),
            Err(_) => std::ptr::null_mut(),
        },
        Err(_) => std::ptr::null_mut(),
    }
}

// Serialize an error message to JSON.
fn error_json(msg: &str) -> *mut c_char {
    #[derive(Serialize)]
    struct ErrWrap<'a> {
        error: &'a str,
    }
    to_json_c_string(&ErrWrap { error: msg })
}

// C ABI: get KER-like target for a node.
#[no_mangle]
pub unsafe extern "C" fn cybo_get_ker_for_node(
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
        Err(_) => return error_json("failed to open SQLite spine"),
    };
    match query_ker_for_node(&conn, node) {
        Ok(row) => to_json_c_string(&row),
        Err(_) => error_json("node not found or no substrate windows"),
    }
}

// C ABI: get blast-radius summary for a node.
#[no_mangle]
pub unsafe extern "C" fn cybo_get_blastradius_for_node(
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
        Err(_) => return error_json("failed to open SQLite spine"),
    };
    match query_blastradius_for_node(&conn, node) {
        Ok(rows) => to_json_c_string(&rows),
        Err(_) => error_json("blast-radius query failed"),
    }
}

// C ABI: get workload windows for a node.
#[no_mangle]
pub unsafe extern "C" fn cybo_get_workload_windows_for_node(
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
        Err(_) => return error_json("failed to open SQLite spine"),
    };
    match query_workload_windows_for_node(&conn, node) {
        Ok(rows) => to_json_c_string(&rows),
        Err(_) => error_json("workload window query failed"),
    }
}

// C ABI: get biodegradable substrate summary for a node.
#[no_mangle]
pub unsafe extern "C" fn cybo_get_substrate_summary_for_node(
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
        Err(_) => return error_json("failed to open SQLite spine"),
    };
    match query_substrate_summary_for_node(&conn, node) {
        Ok(rows) => to_json_c_string(&rows),
        Err(_) => error_json("substrate summary query failed"),
    }
}

// C ABI: free JSON strings returned by this library.
#[no_mangle]
pub unsafe extern "C" fn cybo_free_json(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    let _ = CString::from_raw(ptr);
}
