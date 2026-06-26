// filename: cyboquatic_index/src/lib.rs
// destination: eco_restoration_shard/cyboquatic_index/src/lib.rs

pub mod migration;
pub mod api;

use rusqlite::Connection;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

fn open_connection(path: &str) -> Option<Connection> {
    Connection::open(path).ok()
}

#[no_mangle]
pub extern "C" fn cybo_index_list_energy_carbon_json(
    db_path: *const c_char,
    asset_code: *const c_char,
    t_start: *const c_char,
    t_end: *const c_char,
) -> *mut c_char {
    if db_path.is_null() || asset_code.is_null() || t_start.is_null() || t_end.is_null() {
        return std::ptr::null_mut();
    }
    let db = unsafe { CStr::from_ptr(db_path) }.to_str().ok();
    let ac = unsafe { CStr::from_ptr(asset_code) }.to_str().ok();
    let ts = unsafe { CStr::from_ptr(t_start) }.to_str().ok();
    let te = unsafe { CStr::from_ptr(t_end) }.to_str().ok();
    let (db, ac, ts, te) = match (db, ac, ts, te) {
        (Some(a), Some(b), Some(c), Some(d)) => (a, b, c, d),
        _ => return std::ptr::null_mut(),
    };

    let conn = match open_connection(db) {
        Some(c) => c,
        None => return std::ptr::null_mut(),
    };

    let res = match api::list_energy_carbon_windows(&conn, ac, ts, te) {
        Ok(v) => v,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&res) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(json).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn cybo_index_energy_cost_hint_json(
    db_path: *const c_char,
    asset_code: *const c_char,
    region_code: *const c_char,
    t_start: *const c_char,
    t_end: *const c_char,
) -> *mut c_char {
    if db_path.is_null()
        || asset_code.is_null()
        || region_code.is_null()
        || t_start.is_null()
        || t_end.is_null()
    {
        return std::ptr::null_mut();
    }

    let db = unsafe { CStr::from_ptr(db_path) }.to_str().ok();
    let ac = unsafe { CStr::from_ptr(asset_code) }.to_str().ok();
    let rc = unsafe { CStr::from_ptr(region_code) }.to_str().ok();
    let ts = unsafe { CStr::from_ptr(t_start) }.to_str().ok();
    let te = unsafe { CStr::from_ptr(t_end) }.to_str().ok();
    let (db, ac, rc, ts, te) = match (db, ac, rc, ts, te) {
        (Some(a), Some(b), Some(c), Some(d), Some(e)) => (a, b, c, d, e),
        _ => return std::ptr::null_mut(),
    };

    let conn = match open_connection(db) {
        Some(c) => c,
        None => return std::ptr::null_mut(),
    };

    let res = match api::estimate_energy_cost_shift(&conn, ac, rc, ts, te) {
        Ok(Some(hint)) => hint,
        _ => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&res) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(json).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn cybo_index_free(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
