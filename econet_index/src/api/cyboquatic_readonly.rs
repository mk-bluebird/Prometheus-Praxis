// filename: econet_index/src/api/cyboquatic_readonly.rs

//! Readonly helper API on top of the Cyboquatic blast‑radius and workload spine.
//! Intended for FFI exposure to Lua, C++, and Kotlin/Android as JSON strings.
//! Non‑actuating by design: only queries SQLite and summarizes diagnostics.

use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlastRadiusLink {
    pub link_id: i64,
    pub source_type: String,
    pub source_id: i64,
    pub target_type: String,
    pub target_id: String,
    pub impact_type: String,
    pub impact_score: f64,
    pub vt_sensitivity: Option<f64>,
    pub notes: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadWindowSummary {
    pub node_id: String,
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
    pub mean_r_carbon: f64,
    pub mean_r_biodiv: f64,
    pub accept_fraction: f64,
}

/// List blast‑radius links for a given Cyboquatic shard.
pub fn list_blastradius_for_shard(conn: &Connection, shard_id: i64) -> rusqlite::Result<Vec<BlastRadiusLink>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            link_id,
            source_type,
            source_id,
            target_type,
            target_id,
            impact_type,
            impact_score,
            vt_sensitivity,
            notes
        FROM blastradius_link
        WHERE source_type = 'SHARD'
          AND source_id = ?1
        ORDER BY impact_type, target_type, target_id
        "#,
    )?;

    let iter = stmt.query_map(params![shard_id], |row| {
        Ok(BlastRadiusLink {
            link_id: row.get(0)?,
            source_type: row.get(1)?,
            source_id: row.get(2)?,
            target_type: row.get(3)?,
            target_id: row.get(4)?,
            impact_type: row.get(5)?,
            impact_score: row.get(6)?,
            vt_sensitivity: row.get(7)?,
            notes: row.get(8)?,
        })
    })?;

    let mut out = Vec::new();
    for br in iter {
        out.push(br?);
    }
    Ok(out)
}

/// Summarize Cyboquatic workload behaviour for a node in a given time window.
pub fn summarize_workload_window(
    conn: &Connection,
    node_id: &str,
    t_start_utc: &str,
    t_end_utc: &str,
) -> rusqlite::Result<WorkloadWindowSummary> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            COALESCE(SUM(e_req_j), 0.0)                                         AS total_requests_j,
            COALESCE(SUM(e_surplus_j), 0.0)                                     AS total_surplus_j,
            COALESCE(SUM(CASE WHEN decision = 'ACCEPT'  THEN e_req_j ELSE 0 END), 0.0) AS accepted_requests_j,
            COALESCE(SUM(CASE WHEN decision = 'REJECT'  THEN e_req_j ELSE 0 END), 0.0) AS rejected_requests_j,
            COALESCE(SUM(CASE WHEN decision = 'REROUTE' THEN e_req_j ELSE 0 END), 0.0) AS rerouted_requests_j,
            COALESCE(AVG(vt_before), 0.0)                                       AS mean_vt_before,
            COALESCE(AVG(vt_after), 0.0)                                        AS mean_vt_after,
            COALESCE(AVG(vt_after - vt_before), 0.0)                            AS mean_delta_vt,
            COALESCE(AVG(r_carbon), 0.0)                                        AS mean_r_carbon,
            COALESCE(AVG(r_biodiv), 0.0)                                        AS mean_r_biodiv,
            COALESCE(
                CAST(SUM(CASE WHEN decision = 'ACCEPT' THEN 1 ELSE 0 END) AS REAL)
                / NULLIF(COUNT(*), 0),
                0.0
            )                                                                    AS accept_fraction,
            COALESCE(MIN(timestamp_utc), ?2)                                     AS window_start_utc,
            COALESCE(MAX(timestamp_utc), ?3)                                     AS window_end_utc
        FROM cybo_workload_ledger
        WHERE node_id = ?1
          AND timestamp_utc >= ?2
          AND timestamp_utc <= ?3
        "#,
    )?;

    let row = stmt.query_row(params![node_id, t_start_utc, t_end_utc], |row| {
        Ok(WorkloadWindowSummary {
            node_id: node_id.to_string(),
            window_start_utc: row.get(11)?,
            window_end_utc: row.get(12)?,
            total_requests_j: row.get(0)?,
            total_surplus_j: row.get(1)?,
            accepted_requests_j: row.get(2)?,
            rejected_requests_j: row.get(3)?,
            rerouted_requests_j: row.get(4)?,
            mean_vt_before: row.get(5)?,
            mean_vt_after: row.get(6)?,
            mean_delta_vt: row.get(7)?,
            mean_r_carbon: row.get(8)?,
            mean_r_biodiv: row.get(9)?,
            accept_fraction: row.get(10)?,
        })
    })?;

    Ok(row)
}

//
// FFI surface (C ABI) for Lua, C++, Kotlin/Android
//

#[no_mangle]
pub extern "C" fn cybo_list_blastradius_for_shard_json(
    db_path_ptr: *const libc::c_char,
    shard_id: i64,
) -> *mut libc::c_char {
    use std::ffi::{CStr, CString};

    if db_path_ptr.is_null() {
        return std::ptr::null_mut();
    }

    let c_str = unsafe { CStr::from_ptr(db_path_ptr) };
    let db_path = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let conn = match Connection::open(db_path) {
        Ok(c) => c,
        Err(_) => return std::ptr::null_mut(),
    };

    let links = match list_blastradius_for_shard(&conn, shard_id) {
        Ok(v) => v,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&links) {
        Ok(j) => j,
        Err(_) => return std::ptr::null_mut(),
    };

    let c_json = match CString::new(json) {
        Ok(c) => c,
        Err(_) => return std::ptr::null_mut(),
    };

    c_json.into_raw()
}

#[no_mangle]
pub extern "C" fn cybo_summarize_workload_window_json(
    db_path_ptr: *const libc::c_char,
    node_id_ptr: *const libc::c_char,
    t_start_ptr: *const libc::c_char,
    t_end_ptr: *const libc::c_char,
) -> *mut libc::c_char {
    use std::ffi::{CStr, CString};

    if db_path_ptr.is_null() || node_id_ptr.is_null() || t_start_ptr.is_null() || t_end_ptr.is_null()
    {
        return std::ptr::null_mut();
    }

    let db_path = unsafe { CStr::from_ptr(db_path_ptr) }.to_str().ok()?;
    let node_id = unsafe { CStr::from_ptr(node_id_ptr) }.to_str().ok()?;
    let t_start = unsafe { CStr::from_ptr(t_start_ptr) }.to_str().ok()?;
    let t_end = unsafe { CStr::from_ptr(t_end_ptr) }.to_str().ok()?;

    let conn = match Connection::open(db_path) {
        Ok(c) => c,
        Err(_) => return std::ptr::null_mut(),
    };

    let summary = match summarize_workload_window(&conn, node_id, t_start, t_end) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&summary) {
        Ok(j) => j,
        Err(_) => return std::ptr::null_mut(),
    };

    let c_json = match CString::new(json) {
        Ok(c) => c,
        Err(_) => return std::ptr::null_mut(),
    };

    c_json.into_raw()
}
