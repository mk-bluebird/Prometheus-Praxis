// filename: ffi.rs
// destination: ecorestoration_shard/blast_radius_kernel/src/ffi.rs

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

use rusqlite::Connection;
use serde_json::to_string;

use crate::lambda_compute::{compute_lambda_for_segment, list_latest_lambda_for_region};
use crate::model::LambdaQuery;

/// Convert a C string pointer to Rust String, returning None if null or invalid UTF-8.
fn cstr_to_string(ptr: *const c_char) -> Option<String> {
    if ptr.is_null() {
        return None;
    }
    unsafe {
        CStr::from_ptr(ptr)
            .to_str()
            .ok()
            .map(|s| s.to_string())
    }
}

/// Allocate a C string from a Rust &str.
fn string_to_cstring_ptr(s: &str) -> *mut c_char {
    CString::new(s)
        .map(|c| c.into_raw())
        .unwrap_or_else(|_| std::ptr::null_mut())
}

/// FFI function to compute lambda for a given segment and return JSON.
/// JSON schema matches LambdaSummary.
#[no_mangle]
pub extern "C" fn eco_lambda_for_segment_json(
    db_path: *const c_char,
    segment_id: i64,
    region_code: *const c_char,
    contaminant_code: *const c_char,
    season_code: *const c_char,
    temp_celsius: f64,
) -> *mut c_char {
    let db_path_str = match cstr_to_string(db_path) {
        Some(s) => s,
        None => return std::ptr::null_mut(),
    };

    let region_str = match cstr_to_string(region_code) {
        Some(s) => s,
        None => return std::ptr::null_mut(),
    };

    let contaminant_str = match cstr_to_string(contaminant_code) {
        Some(s) => s,
        None => return std::ptr::null_mut(),
    };

    let season_str = match cstr_to_string(season_code) {
        Some(s) => s,
        None => return std::ptr::null_mut(),
    };

    let conn = match Connection::open(&db_path_str) {
        Ok(c) => c,
        Err(_) => return std::ptr::null_mut(),
    };

    let query = LambdaQuery {
        segment_id,
        region_code: region_str,
        contaminant_code: contaminant_str,
        season_code: season_str,
        temp_celsius,
    };

    let summary = match compute_lambda_for_segment(&conn, &query) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match to_string(&summary) {
        Ok(j) => j,
        Err(_) => return std::ptr::null_mut(),
    };

    string_to_cstring_ptr(&json)
}

/// FFI function to list latest lambda summaries for a region.
/// Returns a JSON array of LambdaSummary.
#[no_mangle]
pub extern "C" fn eco_lambda_for_region_json(
    db_path: *const c_char,
    region_code: *const c_char,
) -> *mut c_char {
    let db_path_str = match cstr_to_string(db_path) {
        Some(s) => s,
        None => return std::ptr::null_mut(),
    };

    let region_str = match cstr_to_string(region_code) {
        Some(s) => s,
        None => return std::ptr::null_mut(),
    };

    let conn = match Connection::open(&db_path_str) {
        Ok(c) => c,
        Err(_) => return std::ptr::null_mut(),
    };

    let summaries = match list_latest_lambda_for_region(&conn, &region_str) {
        Ok(v) => v,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match to_string(&summaries) {
        Ok(j) => j,
        Err(_) => return std::ptr::null_mut(),
    };

    string_to_cstring_ptr(&json)
}

/// FFI helper to free strings returned by eco_lambda_* functions.
#[no_mangle]
pub extern "C" fn eco_blast_radius_free_cstring(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
