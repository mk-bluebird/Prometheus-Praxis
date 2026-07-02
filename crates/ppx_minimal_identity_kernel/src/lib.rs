// filename: ppx_minimal_identity_kernel/src/lib.rs
// repo: eco_restoration_shard/ppx_minimal_identity_kernel/src/lib.rs
// Rust 2024, rust-version = "1.85", MIT OR Apache-2.0

pub mod migration;
pub mod api;

use rusqlite::{Connection, OpenFlags};
use serde::{Deserialize, Serialize};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use thiserror::Error;

fn open_connection_readonly(db_path: &str) -> Option<Connection> {
    if db_path.is_empty() {
        return None;
    }
    let flags = OpenFlags::SQLITE_OPEN_READONLY | OpenFlags::SQLITE_OPEN_NOMUTEX;
    Connection::open_with_flags(db_path, flags).ok()
}

fn cstr_to_str<'a>(ptr: *const c_char) -> Option<&'a str> {
    if ptr.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(ptr) }.to_str().ok()
}

fn to_json_cstring<T: serde::Serialize>(value: &T) -> *mut c_char {
    match serde_json::to_string(value) {
        Ok(json) => match CString::new(json) {
            Ok(cstr) => cstr.into_raw(),
            Err(_) => ptr::null_mut(),
        },
        Err(_) => ptr::null_mut(),
    }
}

fn error_json_internal(msg: &str) -> *mut c_char {
    let payload = serde_json::json!({ "error": msg }).to_string();
    match CString::new(payload) {
        Ok(cstr) => cstr.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn ppx_id_list_psych_continuity_evidence_json(
    db_path: *const c_char,
    subject_did: *const c_char,
) -> *mut c_char {
    let db_path_str = match cstr_to_str(db_path) {
        Some(s) => s,
        None => return error_json_internal("Invalid db_path pointer"),
    };
    let did_str = match cstr_to_str(subject_did) {
        Some(s) => s,
        None => return error_json_internal("Invalid subject_did pointer"),
    };
    if db_path_str.is_empty() || did_str.is_empty() {
        return error_json_internal("db_path and subject_did must not be empty");
    }

    let conn = match open_connection_readonly(db_path_str) {
        Some(c) => c,
        None => return error_json_internal("Failed to open SQLite connection"),
    };

    let evidence = match api::list_psych_continuity_evidence(&conn, did_str) {
        Ok(v) => v,
        Err(e) => return error_json_internal(&format!("Query error: {e}")),
    };

    to_json_cstring(&evidence)
}

#[no_mangle]
pub extern "C" fn ppx_id_list_neuroright_corridors_json(
    db_path: *const c_char,
    context_tag: *const c_char,
) -> *mut c_char {
    let db_path_str = match cstr_to_str(db_path) {
        Some(s) => s,
        None => return error_json_internal("Invalid db_path pointer"),
    };
    let ctx_str = match cstr_to_str(context_tag) {
        Some(s) => s,
        None => return error_json_internal("Invalid context_tag pointer"),
    };
    if db_path_str.is_empty() || ctx_str.is_empty() {
        return error_json_internal("db_path and context_tag must not be empty");
    }

    let conn = match open_connection_readonly(db_path_str) {
        Some(c) => c,
        None => return error_json_internal("Failed to open SQLite connection"),
    };

    let specs = match api::list_neuroright_corridor_specs_for_context(&conn, ctx_str) {
        Ok(v) => v,
        Err(e) => return error_json_internal(&format!("Query error: {e}")),
    };

    to_json_cstring(&specs)
}

#[no_mangle]
pub extern "C" fn ppx_id_list_system_wellbeing_json(
    db_path: *const c_char,
    system_id: *const c_char,
    context_tag: *const c_char,
) -> *mut c_char {
    let db_path_str = match cstr_to_str(db_path) {
        Some(s) => s,
        None => return error_json_internal("Invalid db_path pointer"),
    };
    let sys_str = match cstr_to_str(system_id) {
        Some(s) => s,
        None => return error_json_internal("Invalid system_id pointer"),
    };
    let ctx_str = match cstr_to_str(context_tag) {
        Some(s) => s,
        None => return error_json_internal("Invalid context_tag pointer"),
    };
    if db_path_str.is_empty() || sys_str.is_empty() || ctx_str.is_empty() {
        return error_json_internal("db_path, system_id, and context_tag must not be empty");
    }

    let conn = match open_connection_readonly(db_path_str) {
        Some(c) => c,
        None => return error_json_internal("Failed to open SQLite connection"),
    };

    let comps = match api::list_system_wellbeing_components(&conn, sys_str, ctx_str) {
        Ok(v) => v,
        Err(e) => return error_json_internal(&format!("Query error: {e}")),
    };

    to_json_cstring(&comps)
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

#[derive(Debug, Error)]
pub enum IdentitySpineError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),

    #[error("UTF-8 conversion error")]
    Utf8,

    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),

    #[error("Invalid argument: {0}")]
    InvalidArg(String),

    #[error("Signer error: {0}")]
    Signer(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StewardIdentityPayload {
    pub steward_did: String,
    pub alt_did_primary: Option<String>,
    pub alt_did_safe_zeta: Option<String>,
    pub alt_eth_address: Option<String>,
    pub role_band: String,
    pub lane: String,
    pub region: String,
    pub issued_at_utc: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StewardIdentityEnvelope {
    pub payload: StewardIdentityPayload,
    pub signature_hex: String,
    pub signer_key_id: String,
}

#[repr(C)]
pub struct ShardIndex {
    conn: Connection,
    identity: StewardIdentityEnvelope,
}

impl ShardIndex {
    fn new(db_path: &str, identity: StewardIdentityEnvelope) -> Result<Self, IdentitySpineError> {
        if db_path.is_empty() {
            return Err(IdentitySpineError::InvalidArg(
                "db_path must not be empty".to_string(),
            ));
        }
        let flags = OpenFlags::SQLITE_OPEN_READONLY | OpenFlags::SQLITE_OPEN_NOMUTEX;
        let conn = Connection::open_with_flags(db_path, flags)?;
        Ok(ShardIndex { conn, identity })
    }
}

fn canonical_identity_json(payload: &StewardIdentityPayload) -> Result<String, IdentitySpineError> {
    serde_json::to_string(payload).map_err(IdentitySpineError::Json)
}

fn sign_identity_payload(
    payload: &StewardIdentityPayload,
) -> Result<StewardIdentityEnvelope, IdentitySpineError> {
    let message = canonical_identity_json(payload)?;
    let (signature_hex, signer_key_id) =
        bostrom_sign(&message).map_err(|e| IdentitySpineError::Signer(e))?;
    Ok(StewardIdentityEnvelope {
        payload: payload.clone(),
        signature_hex,
        signer_key_id,
    })
}

#[no_mangle]
pub extern "C" fn ppx_minimal_identity_kernel_open(
    db_path: *const c_char,
    steward_did: *const c_char,
    role_band: *const c_char,
    lane: *const c_char,
    region: *const c_char,
    issued_at_utc: *const c_char,
    alt_did_primary: *const c_char,
    alt_did_safe_zeta: *const c_char,
    alt_eth_address: *const c_char,
) -> *mut ShardIndex {
    let db = match cstr_to_str(db_path) {
        Some(s) => s,
        None => return ptr::null_mut(),
    };
    let did = match cstr_to_str(steward_did) {
        Some(s) => s,
        None => return ptr::null_mut(),
    };
    let band = match cstr_to_str(role_band) {
        Some(s) => s,
        None => return ptr::null_mut(),
    };
    let lane_str = match cstr_to_str(lane) {
        Some(s) => s,
        None => return ptr::null_mut(),
    };
    let region_str = match cstr_to_str(region) {
        Some(s) => s,
        None => return ptr::null_mut(),
    };
    let ts = match cstr_to_str(issued_at_utc) {
        Some(s) => s,
        None => return ptr::null_mut(),
    };

    if did.is_empty()
        || band.is_empty()
        || lane_str.is_empty()
        || region_str.is_empty()
        || ts.is_empty()
    {
        return ptr::null_mut();
    }

    let alt_primary = if alt_did_primary.is_null() {
        None
    } else {
        cstr_to_str(alt_did_primary)
            .map(|s| s.to_string())
    };
    let alt_zeta = if alt_did_safe_zeta.is_null() {
        None
    } else {
        cstr_to_str(alt_did_safe_zeta)
            .map(|s| s.to_string())
    };
    let alt_eth = if alt_eth_address.is_null() {
        None
    } else {
        cstr_to_str(alt_eth_address)
            .map(|s| s.to_string())
    };

    let payload = StewardIdentityPayload {
        steward_did: did.to_string(),
        alt_did_primary: alt_primary,
        alt_did_safe_zeta: alt_zeta,
        alt_eth_address: alt_eth,
        role_band: band.to_string(),
        lane: lane_str.to_string(),
        region: region_str.to_string(),
        issued_at_utc: ts.to_string(),
    };

    let envelope = match sign_identity_payload(&payload) {
        Ok(env) => env,
        Err(_) => return ptr::null_mut(),
    };

    match ShardIndex::new(db, envelope) {
        Ok(handle) => Box::into_raw(Box::new(handle)),
        Err(_) => ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn ppx_minimal_identity_kernel_close(handle: *mut ShardIndex) {
    if handle.is_null() {
        return;
    }
    unsafe {
        let _ = Box::from_raw(handle);
    }
}

#[no_mangle]
pub extern "C" fn ppx_minimal_identity_kernel_get_envelope(
    handle: *mut ShardIndex,
) -> *mut c_char {
    if handle.is_null() {
        return error_json_internal("Invalid null ShardIndex handle");
    }
    let shard = unsafe { &*handle };
    to_json_cstring(&shard.identity)
}
