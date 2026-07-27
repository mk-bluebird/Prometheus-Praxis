// File: crates/prometheus_praxis_sdk/src/ffi.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
//
// C-ABI bindings for high-frequency POD FFI paths.
// These structs and functions allow C++ numeric kernels
// (wastewater pumps, shredders, hammermills) to pass fully
// normalized telemetry and KER coordinates into Rust without
// JSON serialization.
//
// All unsafe is confined to FFI edges; the crate forbids
// unsafe in general, so any FFI must be carefully audited
// and kept minimal. The rest of the SDK is safe Rust.

#![forbid(unsafe_code)]

use crate::lanes::{
    MachineTelemetry,
    TelemetryDomain,
    KerCoordinates,
    LaneConfig,
    GovernanceGateConfig,
    LaneDecision,
    decide_lane,
};
use rust_decimal::Decimal;
use std::ffi::CStr;
use std::os::raw::c_char;

#[repr(C)]
pub struct CMachineTelemetry {
    pub machine_id: *const c_char,
    pub station_id: *const c_char,
    pub domain: i32,
    pub timestamp_utc: *const c_char,

    pub r_hydraulics: f64,
    pub r_energy: f64,
    pub r_uncertainty: f64,
    pub r_reliability: f64,

    pub r_extra_1: f64,
    pub r_extra_1_valid: bool,
    pub r_extra_2: f64,
    pub r_extra_2_valid: bool,

    pub roh: f64,

    pub vt_current: f64,
    pub vt_next: f64,
}

#[repr(C)]
pub struct CKerCoordinates {
    pub k_knowledge: f64,
    pub e_eco_impact: f64,
    pub r_risk: f64,
}

#[repr(C)]
pub struct CLaneConfig {
    pub lane: i32,

    pub roh_ceiling_global: f64,
    pub max_delta_vt: f64,

    pub k_min_research: f64,
    pub k_min_pilot: f64,
    pub k_min_prod: f64,

    pub e_min_research: f64,
    pub e_min_pilot: f64,
    pub e_min_prod: f64,

    pub r_max_research: f64,
    pub r_max_pilot: f64,
    pub r_max_prod: f64,
}

#[repr(C)]
pub struct CGovernanceGateConfig {
    pub corridor_available: bool,
    pub manual_override_allowed: bool,
    pub allow_research_exploration: bool,
}

#[repr(C)]
pub struct CLaneDecision {
    pub lane: i32,
    pub action: i32,
    pub reason_code: *const c_char,
    pub vt_current: f64,
    pub vt_next: f64,
    pub roh: f64,
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

fn to_domain(code: i32) -> TelemetryDomain {
    match code {
        0 => TelemetryDomain::WastewaterPump,
        1 => TelemetryDomain::Shredder,
        2 => TelemetryDomain::Hammermill,
        3 => TelemetryDomain::Conveyance,
        4 => TelemetryDomain::Magnet,
        _ => TelemetryDomain::Unknown,
    }
}

fn to_lane(code: i32) -> crate::lanes::ActionLane {
    match code {
        0 => crate::lanes::ActionLane::Research,
        1 => crate::lanes::ActionLane::Pilot,
        2 => crate::lanes::ActionLane::Production,
        _ => crate::lanes::ActionLane::Research,
    }
}

fn to_action_code(action: &crate::lanes::LaneAction) -> i32 {
    match action {
        crate::lanes::LaneAction::Proceed => 0,
        crate::lanes::LaneAction::Derate => 1,
        crate::lanes::LaneAction::Halt => 2,
    }
}

/// Convert C string pointer to owned Rust String.
/// Returns empty string on null or invalid UTF-8.
fn cstr_to_string(ptr: *const c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }
    unsafe {
        CStr::from_ptr(ptr)
            .to_string_lossy()
            .into_owned()
    }
}

/// Convert CMachineTelemetry into MachineTelemetry.
fn convert_machine_telemetry(ct: &CMachineTelemetry) -> MachineTelemetry {
    let machine_id = cstr_to_string(ct.machine_id);
    let station_id = cstr_to_string(ct.station_id);
    let timestamp_utc = cstr_to_string(ct.timestamp_utc);

    let r_hydraulics = Decimal::from_f64(ct.r_hydraulics).unwrap_or(Decimal::ZERO);
    let r_energy = Decimal::from_f64(ct.r_energy).unwrap_or(Decimal::ZERO);
    let r_uncertainty = Decimal::from_f64(ct.r_uncertainty).unwrap_or(Decimal::ZERO);
    let r_reliability = Decimal::from_f64(ct.r_reliability).unwrap_or(Decimal::ZERO);

    let r_extra_1 = if ct.r_extra_1_valid {
        Some(Decimal::from_f64(ct.r_extra_1).unwrap_or(Decimal::ZERO))
    } else {
        None
    };

    let r_extra_2 = if ct.r_extra_2_valid {
        Some(Decimal::from_f64(ct.r_extra_2).unwrap_or(Decimal::ZERO))
    } else {
        None
    };

    let roh = Decimal::from_f64(ct.roh).unwrap_or(Decimal::ZERO);
    let vt_current = Decimal::from_f64(ct.vt_current).unwrap_or(Decimal::ZERO);
    let vt_next = Decimal::from_f64(ct.vt_next).unwrap_or(Decimal::ZERO);

    MachineTelemetry {
        machine_id,
        station_id,
        domain: to_domain(ct.domain),
        timestamp_utc,
        r_hydraulics,
        r_energy,
        r_uncertainty,
        r_reliability,
        r_extra_1,
        r_extra_2,
        roh,
        vt_current,
        vt_next,
    }
}

/// Convert CKerCoordinates into KerCoordinates.
fn convert_ker_coordinates(ck: &CKerCoordinates) -> KerCoordinates {
    KerCoordinates {
        k_knowledge: Decimal::from_f64(ck.k_knowledge).unwrap_or(Decimal::ZERO),
        e_eco_impact: Decimal::from_f64(ck.e_eco_impact).unwrap_or(Decimal::ZERO),
        r_risk: Decimal::from_f64(ck.r_risk).unwrap_or(Decimal::ZERO),
    }
}

/// Convert CLaneConfig into LaneConfig.
fn convert_lane_config(cl: &CLaneConfig) -> LaneConfig {
    LaneConfig {
        lane: to_lane(cl.lane),
        roh_ceiling_global: Decimal::from_f64(cl.roh_ceiling_global).unwrap_or(Decimal::ONE),
        max_delta_vt: Decimal::from_f64(cl.max_delta_vt).unwrap_or(Decimal::ZERO),
        k_min_research: Decimal::from_f64(cl.k_min_research).unwrap_or(Decimal::ZERO),
        k_min_pilot: Decimal::from_f64(cl.k_min_pilot).unwrap_or(Decimal::ZERO),
        k_min_prod: Decimal::from_f64(cl.k_min_prod).unwrap_or(Decimal::ZERO),
        e_min_research: Decimal::from_f64(cl.e_min_research).unwrap_or(Decimal::ZERO),
        e_min_pilot: Decimal::from_f64(cl.e_min_pilot).unwrap_or(Decimal::ZERO),
        e_min_prod: Decimal::from_f64(cl.e_min_prod).unwrap_or(Decimal::ZERO),
        r_max_research: Decimal::from_f64(cl.r_max_research).unwrap_or(Decimal::ONE),
        r_max_pilot: Decimal::from_f64(cl.r_max_pilot).unwrap_or(Decimal::ONE),
        r_max_prod: Decimal::from_f64(cl.r_max_prod).unwrap_or(Decimal::ONE),
    }
}

/// Convert CGovernanceGateConfig into GovernanceGateConfig.
fn convert_gate_config(cg: &CGovernanceGateConfig) -> GovernanceGateConfig {
    GovernanceGateConfig {
        corridor_available: cg.corridor_available,
        manual_override_allowed: cg.manual_override_allowed,
        allow_research_exploration: cg.allow_research_exploration,
    }
}

/// Exported C-ABI function to perform lane decision for a single window.
///
/// C++ callers populate CMachineTelemetry, CKerCoordinates,
/// CLaneConfig, and CGovernanceGateConfig, then pass pointers here.
/// The result is written into out_decision.
#[no_mangle]
pub extern "C" fn prometheus_praxis_decide_lane(
    telemetry: *const CMachineTelemetry,
    ker: *const CKerCoordinates,
    lane_cfg: *const CLaneConfig,
    gate_cfg: *const CGovernanceGateConfig,
    out_decision: *mut CLaneDecision,
) -> i32 {
    if telemetry.is_null() || ker.is_null() || lane_cfg.is_null() || gate_cfg.is_null() || out_decision.is_null() {
        return -1;
    }

    let ct = unsafe { &*telemetry };
    let ck = unsafe { &*ker };
    let cl = unsafe { &*lane_cfg };
    let cg = unsafe { &*gate_cfg };

    let mt = convert_machine_telemetry(ct);
    let ker_coords = convert_ker_coordinates(ck);
    let lane_cfg_rust = convert_lane_config(cl);
    let gate_cfg_rust = convert_gate_config(cg);

    let decision = decide_lane(&mt, &ker_coords, &lane_cfg_rust, &gate_cfg_rust);

    let lane_code = match decision.lane {
        crate::lanes::ActionLane::Research => 0,
        crate::lanes::ActionLane::Pilot => 1,
        crate::lanes::ActionLane::Production => 2,
    };

    let action_code = to_action_code(&decision.action);

    let reason_string = decision.reason_code;
    let reason_cstring = std::ffi::CString::new(reason_string).unwrap_or_else(|_| std::ffi::CString::new("invalid_reason").unwrap());
    let reason_ptr = reason_cstring.as_ptr();

    unsafe {
        let out = &mut *out_decision;
        out.lane = lane_code;
        out.action = action_code;
        out.reason_code = reason_ptr;
        out.vt_current = decision.vt_current.to_f64().unwrap_or(0.0);
        out.vt_next = decision.vt_next.to_f64().unwrap_or(0.0);
        out.roh = decision.roh.to_f64().unwrap_or(0.0);
        out.k = decision.k.to_f64().unwrap_or(0.0);
        out.e = decision.e.to_f64().unwrap_or(0.0);
        out.r = decision.r.to_f64().unwrap_or(0.0);
    }

    0
}
