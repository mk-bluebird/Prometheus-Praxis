// filename: crates/econet_governance_spine/src/lane_governance.rs
// Rust 2024, rust-version = "1.85", MIT OR Apache-2.0
#![forbid(unsafe_code)]

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::{GovernanceSpine, LaneStatus, SpineError};
use serde::Serialize;

/// Result of a lane-admissibility check.
#[derive(Debug, Serialize)]
pub struct LaneAdmissibilityResult {
    pub shardid: String,
    pub region: String,
    pub lane: String,
    pub verdict: String,
    pub admissible: bool,
    pub reason: String,
    pub carbonnegativeok: bool,
    pub restorationok: bool,
    pub residualvt: f64,
}

/// Convert C string pointer to &str.
fn cstr_to_str(ptr: *const c_char) -> Result<&'static str, SpineError> {
    if ptr.is_null() {
        return Err(SpineError::InvalidArgument(
            "Null pointer provided".to_string(),
        ));
    }
    unsafe { CStr::from_ptr(ptr) }
        .to_str()
        .map_err(|_| SpineError::Utf8)
}

/// Error JSON helper.
fn error_json_internal(msg: &str) -> *mut c_char {
    let payload = serde_json::json!({ "error": msg }).to_string();
    match CString::new(payload) {
        Ok(cstr) => cstr.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Serialize any payload to JSON C string.
fn to_json_cstring<T: Serialize>(value: &T) -> *mut c_char {
    match serde_json::to_string(value) {
        Ok(json) => match CString::new(json) {
            Ok(cstr) => cstr.into_raw(),
            Err(_) => ptr::null_mut(),
        },
        Err(_) => ptr::null_mut(),
    }
}

/// Pure lane-governance guard using LaneStatus.
pub fn check_lane_admissibility(
    lane_status: &LaneStatus,
    now_utc_secs: i64,
) -> LaneAdmissibilityResult {
    let mut admissible = true;
    let mut reason = String::new();

    if lane_status.verdict != "ADMISSIBLE" {
        admissible = false;
        reason = format!("Lane verdict is {}", lane_status.verdict);
    }

    if !lane_status.carbonnegativeok {
        admissible = false;
        if !reason.is_empty() {
            reason.push_str("; ");
        }
        reason.push_str("carbonnegativeok is false");
    }

    if !lane_status.restorationok {
        admissible = false;
        if !reason.is_empty() {
            reason.push_str("; ");
        }
        reason.push_str("restorationok is false");
    }

    // Staleness / expiry check.
    if lane_status.expiresutc < now_utc_secs {
        admissible = false;
        if !reason.is_empty() {
            reason.push_str("; ");
        }
        reason.push_str("lane status is expired");
    }

    // Optionally restrict write-facing operations to specific lanes.
    if lane_status.lane != "EXPPROD" && lane_status.lane != "PROD" {
        admissible = false;
        if !reason.is_empty() {
            reason.push_str("; ");
        }
        reason.push_str("lane is not EXPPROD/PROD for write-facing operation");
    }

    if reason.is_empty() {
        reason = "lane admissible".to_string();
    }

    LaneAdmissibilityResult {
        shardid: lane_status.shardid.clone(),
        region: lane_status.region.clone(),
        lane: lane_status.lane.clone(),
        verdict: lane_status.verdict.clone(),
        admissible,
        reason,
        carbonnegativeok: lane_status.carbonnegativeok,
        restorationok: lane_status.restorationok,
        residualvt: lane_status.residualvt,
    }
}

/// FFI: check lane admissibility for a given shardid.
/// db_path: path to EcoNet DB.
/// shardid: shard identifier.
///
/// Returns JSON LaneAdmissibilityResult or error JSON.
#[no_mangle]
pub extern "C" fn econet_lane_governance_check(
    db_path: *const c_char,
    shardid: *const c_char,
) -> *mut c_char {
    let db = match cstr_to_str(db_path) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&format!("db_path error: {e}")),
    };
    let shard = match cstr_to_str(shardid) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&format!("shardid error: {e}")),
    };

    if shard.is_empty() {
        return error_json_internal("Shard ID cannot be empty");
    }

    // Load expected schema and open GovernanceSpine.
    let expected = crate::schema::loadexpectedschema();
    let path_buf = std::path::PathBuf::from(db);
    let spine = match GovernanceSpine::opendb(path_buf, expected) {
        Ok(s) => s,
        Err(e) => return error_json_internal(&format!("GovernanceSpine error: {e}")),
    };

    let lane_status = match spine.get_lanestatus(shard) {
        Ok(ls) => ls,
        Err(e) => return error_json_internal(&format!("get_lanestatus error: {e}")),
    };

    let now_secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs() as i64;

    let result = check_lane_admissibility(&lane_status, now_secs);
    to_json_cstring(&result)
}
