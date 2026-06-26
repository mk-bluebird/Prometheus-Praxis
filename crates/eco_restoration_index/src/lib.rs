// filename: eco_restoration_index/src/lib.rs

pub mod migration;
pub mod api;

use rusqlite::Connection;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

fn open_connection(db_path: &str) -> Option<Connection> {
    Connection::open(db_path).ok()
}

#[no_mangle]
pub extern "C" fn eco_index_list_blastradius_for_shard_json(
    db_path: *const c_char,
    shardid: i64,
) -> *mut c_char {
    if db_path.is_null() {
        return std::ptr::null_mut();
    }
    let c_str = unsafe { CStr::from_ptr(db_path) };
    let path = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let conn = match open_connection(path) {
        Some(c) => c,
        None => return std::ptr::null_mut(),
    };

    let links = match api::list_blast_radius_for_shard(&conn, shardid) {
        Ok(v) => v,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&links) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    match CString::new(json) {
        Ok(c_string) => c_string.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn eco_index_summarize_workload_window_json(
    db_path: *const c_char,
    nodeid: *const c_char,
    tstart_utc: *const c_char,
    tend_utc: *const c_char,
) -> *mut c_char {
    if db_path.is_null() || nodeid.is_null() || tstart_utc.is_null() || tend_utc.is_null() {
        return std::ptr::null_mut();
    }

    let db_path_str = unsafe { CStr::from_ptr(db_path) }.to_str().ok();
    let nodeid_str = unsafe { CStr::from_ptr(nodeid) }.to_str().ok();
    let tstart_str = unsafe { CStr::from_ptr(tstart_utc) }.to_str().ok();
    let tend_str = unsafe { CStr::from_ptr(tend_utc) }.to_str().ok();

    let (db_path_str, nodeid_str, tstart_str, tend_str) = match (
        db_path_str,
        nodeid_str,
        tstart_str,
        tend_str,
    ) {
        (Some(a), Some(b), Some(c), Some(d)) => (a, b, c, d),
        _ => return std::ptr::null_mut(),
    };

    let conn = match open_connection(db_path_str) {
        Some(c) => c,
        None => return std::ptr::null_mut(),
    };

    let summary = match api::summarize_workload_window(&conn, nodeid_str, tstart_str, tend_str) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&summary) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    match CString::new(json) {
        Ok(c_string) => c_string.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn eco_index_free_cstring(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
