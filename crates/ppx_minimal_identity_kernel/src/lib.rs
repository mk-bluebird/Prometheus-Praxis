// filename: ppx_minimal_identity_kernel/src/lib.rs
// repo: eco_restoration_shard/ppx_minimal_identity_kernel/src/lib.rs

pub mod migration;
pub mod api;

use rusqlite::Connection;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

fn open_connection(db_path: &str) -> Option<Connection> {
    Connection::open(db_path).ok()
}

#[no_mangle]
pub extern "C" fn ppx_id_list_psych_continuity_evidence_json(
    db_path: *const c_char,
    subject_did: *const c_char,
) -> *mut c_char {
    if db_path.is_null() || subject_did.is_null() {
        return std::ptr::null_mut();
    }

    let db_path_str = unsafe { CStr::from_ptr(db_path) }.to_str().ok();
    let did_str = unsafe { CStr::from_ptr(subject_did) }.to_str().ok();
    let (db_path_str, did_str) = match (db_path_str, did_str) {
        (Some(a), Some(b)) => (a, b),
        _ => return std::ptr::null_mut(),
    };

    let conn = match open_connection(db_path_str) {
        Some(c) => c,
        None => return std::ptr::null_mut(),
    };

    let evidence = match api::list_psych_continuity_evidence(&conn, did_str) {
        Ok(v) => v,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&evidence) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(json).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn ppx_id_list_neuroright_corridors_json(
    db_path: *const c_char,
    context_tag: *const c_char,
) -> *mut c_char {
    if db_path.is_null() || context_tag.is_null() {
        return std::ptr::null_mut();
    }

    let db_path_str = unsafe { CStr::from_ptr(db_path) }.to_str().ok();
    let ctx_str = unsafe { CStr::from_ptr(context_tag) }.to_str().ok();
    let (db_path_str, ctx_str) = match (db_path_str, ctx_str) {
        (Some(a), Some(b)) => (a, b),
        _ => return std::ptr::null_mut(),
    };

    let conn = match open_connection(db_path_str) {
        Some(c) => c,
        None => return std::ptr::null_mut(),
    };

    let specs = match api::list_neuroright_corridor_specs_for_context(&conn, ctx_str) {
        Ok(v) => v,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&specs) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(json).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn ppx_id_list_system_wellbeing_json(
    db_path: *const c_char,
    system_id: *const c_char,
    context_tag: *const c_char,
) -> *mut c_char {
    if db_path.is_null() || system_id.is_null() || context_tag.is_null() {
        return std::ptr::null_mut();
    }

    let db_path_str = unsafe { CStr::from_ptr(db_path) }.to_str().ok();
    let sys_str = unsafe { CStr::from_ptr(system_id) }.to_str().ok();
    let ctx_str = unsafe { CStr::from_ptr(context_tag) }.to_str().ok();
    let (db_path_str, sys_str, ctx_str) = match (db_path_str, sys_str, ctx_str) {
        (Some(a), Some(b), Some(c)) => (a, b, c),
        _ => return std::ptr::null_mut(),
    };

    let conn = match open_connection(db_path_str) {
        Some(c) => c,
        None => return std::ptr::null_mut(),
    };

    let comps = match api::list_system_wellbeing_components(&conn, sys_str, ctx_str) {
        Ok(v) => v,
        Err(_) => return std::ptr::null_mut(),
    };

    let json = match serde_json::to_string(&comps) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(json).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn ppx_id_free_cstring(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr);
    }
}
