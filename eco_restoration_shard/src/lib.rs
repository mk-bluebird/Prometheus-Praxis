// filename: src/lib.rs
// destination: eco_restoration_shard/src/lib.rs
// Purpose:
// - Provide a small cdylib exposing read-only JSON APIs over the EcoNet index
//   and the Cyboquatic blast-radius/ledger evidence for Lua and Kotlin/Android.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

use rusqlite::{Connection, OpenFlags};
use serde::Serialize;

#[derive(Debug, Serialize)]
pub struct KerTargets {
    pub repo_name: String,
    pub role_band: String,
    pub ker_target_k: f64,
    pub ker_target_e: f64,
    pub ker_target_r: f64,
}

#[derive(Debug, Serialize)]
pub struct BlastRadiusEntry {
    pub source_type: String,
    pub source_id: String,
    pub target_type: String,
    pub target_id: String,
    pub impact_type: String,
    pub impact_score: f64,
    pub vt_sensitivity: f64,
    pub notes: String,
}

#[derive(Debug, Serialize)]
pub struct WorkloadTrendEntry {
    pub node_id: String,
    pub channel: String,
    pub total_requests_j: f64,
    pub total_surplus_j: f64,
    pub mean_vt_before: f64,
    pub mean_vt_after: f64,
    pub mean_r_carbon: Option<f64>,
    pub mean_r_biodiv: Option<f64>,
}

/// Internal helper: open read-only SQLite connection.
fn open_ro(db_path: &str) -> rusqlite::Result<Connection> {
    Connection::open_with_flags(
        Path::new(db_path),
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
    )
}

/// Internal helper: convert C string pointer to &str safely.
unsafe fn cstr_to_str(ptr: *const c_char) -> Result<&'static str, &'static str> {
    if ptr.is_null() {
        return Err("null pointer");
    }
    CStr::from_ptr(ptr).to_str().map_err(|_| "invalid UTF-8")
}

/// Return repo-level KER targets as JSON for a given repo_name.
fn query_ker_targets(conn: &Connection, repo_name: &str) -> rusqlite::Result<KerTargets> {
    let mut stmt = conn.prepare(
        r#"
        SELECT repo_name, role_band, ker_target_k, ker_target_e, ker_target_r
        FROM econet_repo_index
        WHERE repo_name = ?1
        LIMIT 1
        "#,
    )?;
    stmt.query_row([repo_name], |row| {
        Ok(KerTargets {
            repo_name: row.get(0)?,
            role_band: row.get(1)?,
            ker_target_k: row.get(2)?,
            ker_target_e: row.get(3)?,
            ker_target_r: row.get(4)?,
        })
    })
}

/// Return blast-radius entries for a given node_id.
fn query_blast_radius(conn: &Connection, node_id: &str) -> rusqlite::Result<Vec<BlastRadiusEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT source_type, source_id, target_type, target_id,
               impact_type, impact_score,
               COALESCE(vt_sensitivity, 0.0), COALESCE(notes, '')
        FROM blastradius_link
        WHERE source_type = 'NODE' AND source_id = ?1
        ORDER BY impact_type, target_type, target_id
        "#,
    )?;
    let rows = stmt.query_map([node_id], |row| {
        Ok(BlastRadiusEntry {
            source_type: row.get(0)?,
            source_id: row.get(1)?,
            target_type: row.get(2)?,
            target_id: row.get(3)?,
            impact_type: row.get(4)?,
            impact_score: row.get(5)?,
            vt_sensitivity: row.get(6)?,
            notes: row.get(7)?,
        })
    })?;
    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

/// Return simple workload trend aggregates for a node_id across channels.
fn query_workload_trends(conn: &Connection, node_id: &str) -> rusqlite::Result<Vec<WorkloadTrendEntry>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            node_id,
            channel,
            SUM(e_req_j)              AS total_requests_j,
            SUM(e_surplus_j)          AS total_surplus_j,
            AVG(vt_before)            AS mean_vt_before,
            AVG(vt_after)             AS mean_vt_after,
            AVG(r_carbon)             AS mean_r_carbon,
            AVG(r_biodiv)             AS mean_r_biodiv
        FROM cybo_workload_ledger
        WHERE node_id = ?1
        GROUP BY node_id, channel
        ORDER BY channel
        "#,
    )?;
    let rows = stmt.query_map([node_id], |row| {
        Ok(WorkloadTrendEntry {
            node_id: row.get(0)?,
            channel: row.get(1)?,
            total_requests_j: row.get(2)?,
            total_surplus_j: row.get(3)?,
            mean_vt_before: row.get(4)?,
            mean_vt_after: row.get(5)?,
            mean_r_carbon: row.get(6)?,
            mean_r_biodiv: row.get(7)?,
        })
    })?;
    let mut out = Vec::new();
    for r in rows {
        out.push(r?);
    }
    Ok(out)
}

/// Serialize a value to JSON and hand it out as a C string pointer.
fn to_json_cstring<T: Serialize>(val: &T) -> *mut c_char {
    match serde_json::to_string(val) {
        Ok(json) => CString::new(json).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Serialize an error message to JSON.
fn error_json(msg: &str) -> *mut c_char {
    let wrapped = serde_json::json!({ "error": msg });
    to_json_cstring(&wrapped)
}

/// C ABI: get repo-level KER targets as JSON given a SQLite index path and repo_name.
///
/// Safety:
/// - db_path and repo_name must be valid, null-terminated UTF-8 strings.
/// - Caller must free the returned pointer via econet_free_json.
#[no_mangle]
pub unsafe extern "C" fn econet_get_ker_targets(
    db_path: *const c_char,
    repo_name: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(db_path) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let repo = match cstr_to_str(repo_name) {
        Ok(s) => s,
        Err(m) => return error_json(m),
    };
    let conn = match open_ro(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_ker_targets(&conn, repo) {
        Ok(row) => to_json_cstring(&row),
        Err(_) => error_json("repo not found"),
    }
}

/// C ABI: get blast-radius entries for a node_id as JSON.
///
/// Safety:
/// - db_path and node_id must be valid, null-terminated UTF-8 strings.
/// - Caller must free the returned pointer via econet_free_json.
#[no_mangle]
pub unsafe extern "C" fn econet_get_blastradius_for_node(
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
    let conn = match open_ro(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_blast_radius(&conn, node) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("node not found or query failed"),
    }
}

/// C ABI: get aggregated KER/workload trends for a node_id as JSON.
///
/// Safety:
/// - db_path and node_id must be valid, null-terminated UTF-8 strings.
/// - Caller must free the returned pointer via econet_free_json.
#[no_mangle]
pub unsafe extern "C" fn econet_get_workload_trends_for_node(
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
    let conn = match open_ro(db) {
        Ok(c) => c,
        Err(_) => return error_json("failed to open SQLite index"),
    };
    match query_workload_trends(&conn, node) {
        Ok(rows) => to_json_cstring(&rows),
        Err(_) => error_json("node not found or query failed"),
    }
}

/// C ABI: free JSON strings returned by this library.
///
/// Safety:
/// - ptr must be a pointer previously returned by one of the econet_* functions.
#[no_mangle]
pub unsafe extern "C" fn econet_free_json(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    let _ = CString::from_raw(ptr);
}
