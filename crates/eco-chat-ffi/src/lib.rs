// FILE: crates/eco-chat-ffi/src/lib.rs
// DESTINATION: crates/eco-chat-ffi/src/lib.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard
//
// Readonly, non-actuating tool surface for AI-chat agents.
//
// All public extern "C" functions:
//   - accept raw C-string DB path + optional filter strings
//   - return a heap-allocated C-string containing UTF-8 JSON
//   - callers must free the string with `eco_chat_ffi_free_str`
//
// All Rust-level functions are also exported for in-process callers.

#![forbid(unsafe_code)]
#![warn(missing_docs)]

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::Path;

use rusqlite::{Connection, OpenFlags};
use serde::Serialize;
use thiserror::Error;

use ecoresponseshard::{
    query_prod_high_trust, summarize_workload_window, ProdHighTrustFilter, ResponseShardRow,
    WorkloadWindowSummary,
};

/// Errors from the FFI layer.
#[derive(Debug, Error)]
pub enum FfiError {
    /// Underlying DB error.
    #[error("db: {0}")]
    Db(#[from] rusqlite::Error),
    /// JSON serialisation error.
    #[error("json: {0}")]
    Json(#[from] serde_json::Error),
    /// Response-shard error.
    #[error("responseshard: {0}")]
    ResponseShard(#[from] ecoresponseshard::ResponseShardError),
    /// Null pointer passed from C caller.
    #[error("null pointer argument")]
    NullPtr,
    /// Invalid UTF-8 in C string.
    #[error("invalid utf-8")]
    Utf8(#[from] std::str::Utf8Error),
}

/// Blast-radius summary row.
#[derive(Debug, Serialize)]
pub struct BlastRadiusRow {
    /// Shard identifier.
    pub shard_id: String,
    /// Lane at snapshot time.
    pub lane: String,
    /// Region.
    pub region: String,
    /// Aggregate blast-radius index (0..1, lower is safer).
    pub blast_radius_index: f64,
    /// Vt residual at snapshot.
    pub vt_residual: f64,
}

/// Open a readonly SQLite connection.
fn open_readonly<P: AsRef<Path>>(db_path: P) -> Result<Connection, FfiError> {
    let conn = Connection::open_with_flags(
        db_path,
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_URI,
    )?;
    conn.pragma_update(None, "foreign_keys", "ON")?;
    Ok(conn)
}

/// List blast-radius rows for a given shard from `blast_radius_index` table.
///
/// Returns a JSON array of `BlastRadiusRow`.
pub fn list_blast_radius_for_shard(
    db_path: &str,
    shard_id: &str,
) -> Result<String, FfiError> {
    let conn = open_readonly(db_path)?;

    let sql = r#"
        SELECT
            bri.shard_id,
            bri.lane,
            bri.region,
            bri.blast_radius_index,
            bri.vt_residual
        FROM blast_radius_index AS bri
        WHERE bri.shard_id = ?1
        ORDER BY bri.snapshot_utc DESC
        LIMIT 64
    "#;

    let mut stmt = conn.prepare(sql)?;
    let rows = stmt
        .query_map(rusqlite::params![shard_id], |row| {
            Ok(BlastRadiusRow {
                shard_id:           row.get(0)?,
                lane:               row.get(1)?,
                region:             row.get(2)?,
                blast_radius_index: row.get(3)?,
                vt_residual:        row.get(4)?,
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;

    Ok(serde_json::to_string(&rows)?)
}

/// Return JSON summary for a workload window.
///
/// Delegates to `ecoresponseshard::summarize_workload_window`.
pub fn summarize_workload_window_tool(
    db_path: &str,
    shard_id: &str,
    window_start_utc: &str,
    window_end_utc: &str,
) -> Result<String, FfiError> {
    summarize_workload_window(db_path, shard_id, window_start_utc, window_end_utc)
        .map_err(FfiError::ResponseShard)
        .and_then(|v| serde_json::to_string(&v).map_err(FfiError::Json))
}

/// Return JSON array of PROD+HIGH-trust response-shard rows.
///
/// Delegates to `ecoresponseshard::query_prod_high_trust`.
pub fn query_prod_high_trust_tool(
    db_path: &str,
    region: Option<&str>,
    limit: u32,
) -> Result<String, FfiError> {
    let filter = ProdHighTrustFilter {
        region: region.map(str::to_owned),
        limit,
    };
    query_prod_high_trust(db_path, filter)
        .map_err(FfiError::ResponseShard)
        .and_then(|v| serde_json::to_string(&v).map_err(FfiError::Json))
}

// ── C-ABI exports ─────────────────────────────────────────────────────────────
// Each function returns a heap-allocated UTF-8 C string on success,
// or a JSON error object `{"error":"<message>"}` on failure.
// The caller MUST call `eco_chat_ffi_free_str` to release the memory.

unsafe fn cstr_to_str<'a>(ptr: *const c_char) -> Result<&'a str, FfiError> {
    if ptr.is_null() {
        return Err(FfiError::NullPtr);
    }
    // SAFETY: caller guarantees ptr is a valid, null-terminated C string.
    Ok(unsafe { CStr::from_ptr(ptr) }.to_str()?)
}

fn to_c_string(s: String) -> *mut c_char {
    CString::new(s)
        .unwrap_or_else(|_| CString::new("{\"error\":\"cstring conversion failed\"}").unwrap())
        .into_raw()
}

fn error_json(e: &FfiError) -> *mut c_char {
    to_c_string(format!("{{\"error\":\"{}\"}}", e))
}

/// # Safety
/// `db_path` and `shard_id` must be valid, null-terminated C strings.
/// The returned pointer must be freed with `eco_chat_ffi_free_str`.
#[no_mangle]
pub unsafe extern "C" fn eco_list_blast_radius_for_shard(
    db_path: *const c_char,
    shard_id: *const c_char,
) -> *mut c_char {
    let db = match unsafe { cstr_to_str(db_path) } {
        Ok(s) => s,
        Err(e) => return error_json(&e),
    };
    let sid = match unsafe { cstr_to_str(shard_id) } {
        Ok(s) => s,
        Err(e) => return error_json(&e),
    };

    match list_blast_radius_for_shard(db, sid) {
        Ok(json) => to_c_string(json),
        Err(e)   => error_json(&e),
    }
}

/// # Safety
/// All pointer arguments must be valid, null-terminated C strings.
/// `window_start_utc` and `window_end_utc` must be ISO-8601.
/// The returned pointer must be freed with `eco_chat_ffi_free_str`.
#[no_mangle]
pub unsafe extern "C" fn eco_summarize_workload_window(
    db_path:          *const c_char,
    shard_id:         *const c_char,
    window_start_utc: *const c_char,
    window_end_utc:   *const c_char,
) -> *mut c_char {
    let db  = match unsafe { cstr_to_str(db_path) }          { Ok(s) => s, Err(e) => return error_json(&e) };
    let sid = match unsafe { cstr_to_str(shard_id) }         { Ok(s) => s, Err(e) => return error_json(&e) };
    let ws  = match unsafe { cstr_to_str(window_start_utc) } { Ok(s) => s, Err(e) => return error_json(&e) };
    let we  = match unsafe { cstr_to_str(window_end_utc) }   { Ok(s) => s, Err(e) => return error_json(&e) };

    match summarize_workload_window_tool(db, sid, ws, we) {
        Ok(json) => to_c_string(json),
        Err(e)   => error_json(&e),
    }
}

/// # Safety
/// `db_path` must be a valid, null-terminated C string.
/// `region` may be null to query all regions.
/// The returned pointer must be freed with `eco_chat_ffi_free_str`.
#[no_mangle]
pub unsafe extern "C" fn eco_query_prod_high_trust(
    db_path: *const c_char,
    region:  *const c_char,
    limit:   u32,
) -> *mut c_char {
    let db = match unsafe { cstr_to_str(db_path) } {
        Ok(s) => s,
        Err(e) => return error_json(&e),
    };
    let reg: Option<&str> = if region.is_null() {
        None
    } else {
        match unsafe { cstr_to_str(region) } {
            Ok(s) => Some(s),
            Err(e) => return error_json(&e),
        }
    };

    match query_prod_high_trust_tool(db, reg, limit) {
        Ok(json) => to_c_string(json),
        Err(e)   => error_json(&e),
    }
}

/// Free a C string previously returned by this library.
///
/// # Safety
/// `ptr` must have been returned by one of the `eco_*` functions in this
/// library.  Passing any other pointer is undefined behaviour.
#[no_mangle]
pub unsafe extern "C" fn eco_chat_ffi_free_str(ptr: *mut c_char) {
    if !ptr.is_null() {
        // SAFETY: ptr was produced by CString::into_raw inside this crate.
        drop(unsafe { CString::from_raw(ptr) });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn blast_radius_empty_on_fresh_db() {
        // Use :memory: as a stand-in; the table won't exist, so the query
        // returns a SQLite error.  We verify the error path does not panic.
        let result = list_blast_radius_for_shard(":memory:", "shard-1");
        // Either Ok([]) or an Err is acceptable; what must not happen is a panic.
        let _ = result;
    }
}
