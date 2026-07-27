// filename: crates/prometheus_praxis_ai/src/lib.rs

#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use time::OffsetDateTime;

use econet_governance_spine::{ShardIndex, SpineError};
use econet_governance_spine::blastradius::KerBlastRadiusSnapshot;
use econet_governance_spine::laneguard::{LaneAdmissibilityVerdict, lane_check_for_machine};

/// Lane classification for workloads and AI nodes.
///
/// RESEARCH: exploratory, allowed to push residuals for learning.
/// PILOT: constrained experiments, only small residual increases allowed.
/// PRODUCTION: strictly Lyapunov-safe, residual must not increase.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum Lane {
    /// Exploratory lane.
    Research,
    /// Pilot lane.
    Pilot,
    /// Production lane.
    Production,
}

impl Lane {
    /// Parse a lane string as used in ALN particles.
    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "RESEARCH" => Some(Lane::Research),
            "PILOT" => Some(Lane::Pilot),
            "PRODUCTION" => Some(Lane::Production),
            _ => None,
        }
    }

    /// Return the ALN string representation of this lane.
    pub fn as_str(&self) -> &'static str {
        match self {
            Lane::Research => "RESEARCH",
            Lane::Pilot => "PILOT",
            Lane::Production => "PRODUCTION",
        }
    }
}

/// KER triad: Knowledge factor, Eco-impact factor, Risk-of-harm factor.
///
/// Values are always in [0,1] by construction.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct KerTriad {
    /// Knowledge factor (evidence quality / learning value).
    pub k: f64,
    /// Eco-impact factor (positive impact; higher is better).
    pub e: f64,
    /// Risk-of-harm factor (higher implies more risk).
    pub r: f64,
}

impl KerTriad {
    /// Compute ker_score = k * (e - r), consistent with ALN ker-axis.
    pub fn score(&self) -> f64 {
        self.k * (self.e - self.r)
    }

    /// Clamp K, E, R into [0,1] to preserve invariants.
    pub fn clamped(self) -> Self {
        fn clamp01(x: f64) -> f64 {
            if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }

        KerTriad {
            k: clamp01(self.k),
            e: clamp01(self.e),
            r: clamp01(self.r),
        }
    }
}

/// Generic residual slice for a Lyapunov coordinate set.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct ResidualSlice {
    /// Residual before this workload / frame.
    pub vt_before: f64,
    /// Residual after this workload / frame.
    pub vt_after: f64,
    /// Residual change ΔVt = vt_after - vt_before.
    pub delta_vt: f64,
}

impl ResidualSlice {
    /// Construct a residual slice and enforce ΔVt invariants.
    pub fn new(vt_before: f64, vt_after: f64) -> Self {
        let vt_before_norm = if vt_before < 0.0 { 0.0 } else { vt_before };
        let vt_after_norm = if vt_after < 0.0 { 0.0 } else { vt_after };
        let delta_vt = vt_after_norm - vt_before_norm;

        ResidualSlice {
            vt_before: vt_before_norm,
            vt_after: vt_after_norm,
            delta_vt,
        }
    }

    /// Check Lyapunov monotonicity for a given lane.
    ///
    /// - Production: vt_after <= vt_before.
    /// - Pilot: vt_after <= vt_before + epsilon.
    /// - Research: no strict bound (governed upstream).
    pub fn is_monotone_for_lane(&self, lane: Lane, epsilon: f64) -> bool {
        match lane {
            Lane::Production => self.vt_after <= self.vt_before,
            Lane::Pilot => self.vt_after <= self.vt_before + epsilon,
            Lane::Research => true,
        }
    }
}

/// Hydraulics / drainage risk coordinates.
///
/// All coordinates are normalized into [0,1] in accordance with ALN DrainageDecayKernel2026v1.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct DrainageRiskCoords {
    /// Normalized BOD risk.
    pub r_bod: f64,
    /// Normalized TSS risk.
    pub r_tss: f64,
    /// Normalized CEC risk.
    pub r_cec: f64,
    /// Hydraulics / surcharge risk.
    pub r_hydraulics: f64,
    /// Telemetry / model uncertainty risk.
    pub r_uncertainty: f64,
}

impl DrainageRiskCoords {
    /// Clamp all risk coordinates into [0,1].
    pub fn clamped(self) -> Self {
        fn clamp01(x: f64) -> f64 {
            if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }

        DrainageRiskCoords {
            r_bod: clamp01(self.r_bod),
            r_tss: clamp01(self.r_tss),
            r_cec: clamp01(self.r_cec),
            r_hydraulics: clamp01(self.r_hydraulics),
            r_uncertainty: clamp01(self.r_uncertainty),
        }
    }

    /// Compute the drainage residual vt using ALN weights.
    ///
    /// vt = Σ w_j * r_j^2
    pub fn residual(&self) -> f64 {
        const W_BOD: f64 = 0.9;
        const W_TSS: f64 = 0.7;
        const W_CEC: f64 = 0.6;
        const W_HYDRAULICS: f64 = 1.0;
        const W_UNCERTAINTY: f64 = 0.8;

        let r = self.clamped();

        W_BOD * r.r_bod * r.r_bod
            + W_TSS * r.r_tss * r.r_tss
            + W_CEC * r.r_cec * r.r_cec
            + W_HYDRAULICS * r.r_hydraulics * r.r_hydraulics
            + W_UNCERTAINTY * r.r_uncertainty * r.r_uncertainty
    }
}

/// Cyboquatic workload risk coordinates (energetics band).
///
/// Derived from energy tailwind ratio and hydraulics / uncertainty proxies.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct WorkloadRiskCoords {
    /// Energy risk (shortfall vs tailwind).
    pub r_energy: f64,
    /// Hydraulics risk for this workload.
    pub r_hydraulics: f64,
    /// Telemetry / model uncertainty risk.
    pub r_uncertainty: f64,
}

impl WorkloadRiskCoords {
    /// Clamp risk coordinates into [0,1].
    pub fn clamped(self) -> Self {
        fn clamp01(x: f64) -> f64 {
            if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }

        WorkloadRiskCoords {
            r_energy: clamp01(self.r_energy),
            r_hydraulics: clamp01(self.r_hydraulics),
            r_uncertainty: clamp01(self.r_uncertainty),
        }
    }

    /// Compute the workload residual vt using ALN / workload-crate weights.
    pub fn residual(&self) -> f64 {
        const W_ENERGY: f64 = 0.8;
        const W_HYDRAULICS: f64 = 1.0;
        const W_UNCERTAINTY: f64 = 0.6;

        let r = self.clamped();

        W_ENERGY * r.r_energy * r.r_energy
            + W_HYDRAULICS * r.r_hydraulics * r.r_hydraulics
            + W_UNCERTAINTY * r.r_uncertainty * r.r_uncertainty
    }
}

/// AI datacenter node risk coordinates (AI node energetics band).
///
/// These map AI compute and cooling footprints to ecosafety planes.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct AiNodeRiskCoords {
    /// Energy risk for compute workloads.
    pub r_energy_compute: f64,
    /// Cooling / water / hydraulics impact risk.
    pub r_cooling_water: f64,
    /// Carbon intensity risk.
    pub r_carbon: f64,
    /// Local biodiversity / siting impact risk.
    pub r_biodiversity: f64,
    /// Telemetry / model uncertainty risk.
    pub r_uncertainty: f64,
}

impl AiNodeRiskCoords {
    /// Clamp all risk coordinates into [0,1].
    pub fn clamped(self) -> Self {
        fn clamp01(x: f64) -> f64 {
            if x < 0.0 {
                0.0
            } else if x > 1.0 {
                1.0
            } else {
                x
            }
        }

        AiNodeRiskCoords {
            r_energy_compute: clamp01(self.r_energy_compute),
            r_cooling_water: clamp01(self.r_cooling_water),
            r_carbon: clamp01(self.r_carbon),
            r_biodiversity: clamp01(self.r_biodiversity),
            r_uncertainty: clamp01(self.r_uncertainty),
        }
    }

    /// Compute the AI-node residual vt_ai using ALN weights.
    pub fn residual(&self) -> f64 {
        const W_ENERGY_COMPUTE: f64 = 0.7;
        const W_COOLING_WATER: f64 = 0.6;
        const W_CARBON: f64 = 1.0;
        const W_BIODIVERSITY: f64 = 1.0;
        const W_UNCERTAINTY: f64 = 0.8;

        let r = self.clamped();

        W_ENERGY_COMPUTE * r.r_energy_compute * r.r_energy_compute
            + W_COOLING_WATER * r.r_cooling_water * r.r_cooling_water
            + W_CARBON * r.r_carbon * r.r_carbon
            + W_BIODIVERSITY * r.r_biodiversity * r.r_biodiversity
            + W_UNCERTAINTY * r.r_uncertainty * r.r_uncertainty
    }
}

/// Hydraulics / drainage frame governed by DrainageDecayKernel2026v1.
///
/// This struct is the Rust mirror of the ALN particle body and any C++ FFI struct.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DrainageFrame {
    /// Hex-encoded frame identifier.
    pub frame_id: String,
    /// Date in YYYYMMDD.
    pub yyyymmdd: String,
    /// Canal segment id.
    pub canal_segment_id: String,
    /// Node id (physical canal node).
    pub node_id: String,
    /// Biochemical Oxygen Demand [mg/L].
    pub bod_mg_l: f64,
    /// Total Suspended Solids [mg/L].
    pub tss_mg_l: f64,
    /// Cation Exchange Capacity [cmol(+)/kg].
    pub cec_cmol_per_kg: f64,
    /// Flow rate [m3/s].
    pub flow_rate_m3s: f64,
    /// Water temperature [°C].
    pub water_temp_c: f64,
    /// Elevation [m].
    pub elevation_m: f64,
    /// Risk coordinates.
    pub risks: DrainageRiskCoords,
    /// Residual slice.
    pub residual: ResidualSlice,
    /// KER triad.
    pub ker: KerTriad,
    /// Phoenix hex anchor id.
    pub phoenix_hex_anchor: String,
    /// Prior frame id (hex256).
    pub prior_frame_id: String,
}

/// Workload frame governed by WorkloadKernel2026v1.
///
/// This struct mirrors your cyboquatic workload energetics sample.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadFrame {
    /// Hex-encoded frame identifier.
    pub frame_id: String,
    /// Date in YYYYMMDD.
    pub yyyymmdd: String,
    /// Unique workload sample id.
    pub workload_id: String,
    /// Node id (cyboquatic / canal node).
    pub node_id: String,
    /// Task type string.
    pub task_type: String,
    /// Timestamp in ISO-8601 UTC.
    pub timestamputc: String,
    /// Required energy [J].
    pub energyreq_j: f64,
    /// Surplus energy [J].
    pub energysurplus_j: f64,
    /// Hydraulics risk proxy.
    pub hydraulicrisk: f64,
    /// Uncertainty risk proxy.
    pub uncertaintyrisk: f64,
    /// Risk coordinates.
    pub risks: WorkloadRiskCoords,
    /// Residual slice.
    pub residual: ResidualSlice,
    /// KER triad.
    pub ker: KerTriad,
    /// Lane classification.
    pub lane: Lane,
    /// Phoenix hex anchor id.
    pub phoenix_hex_anchor: String,
    /// Prior frame id (hex256).
    pub prior_frame_id: String,
}

/// AI datacenter node frame governed by AiDatacenterNode2026v1.
///
/// This struct binds AI node energetics to the global Lyapunov/KER grammar.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AiNodeFrame {
    /// Hex-encoded frame identifier.
    pub frame_id: String,
    /// Date in YYYYMMDD.
    pub yyyymmdd: String,
    /// Facility id (datacenter / AI campus).
    pub facility_id: String,
    /// Rack or zone id.
    pub rack_id: String,
    /// HeatRiskTile id for plume coupling.
    pub tile_id: String,
    /// Timestamp in ISO-8601 UTC.
    pub timestamputc: String,
    /// Power Usage Effectiveness.
    pub pue: f64,
    /// Cooling Usage Effectiveness.
    pub cue: f64,
    /// IT power draw [kW].
    pub power_kw: f64,
    /// Cooling power draw [kW].
    pub cooling_kw: f64,
    /// Thermal output [kW].
    pub thermal_output_kw: f64,
    /// Throughput (jobs / queries per second).
    pub throughput_qps: f64,
    /// Joules per inference or unit of AI work.
    pub joules_per_inference: f64,
    /// Eco-quota over current window [kWh].
    pub eco_quota_kwh: f64,
    /// Eco-quota window start (ISO-8601 UTC).
    pub eco_quota_window_start_utc: String,
    /// Eco-quota window end (ISO-8601 UTC).
    pub eco_quota_window_end_utc: String,
    /// Upstream HeatGovernanceEvent id.
    pub heat_governance_event_id: String,
    /// Downstream AiLoadScheduleEvent id.
    pub ai_load_schedule_event_id: String,
    /// AI node risk coordinates.
    pub risks: AiNodeRiskCoords,
    /// Residual slice for AI node.
    pub residual_ai: ResidualSlice,
    /// KER triad for AI node slice.
    pub ker: KerTriad,
    /// Lane classification.
    pub lane: Lane,
    /// Phoenix hex anchor id.
    pub phoenix_hex_anchor: String,
    /// Prior frame id (hex256).
    pub prior_frame_id: String,
}

/// Compute KER triad from risk and residual behaviour for workloads.
///
/// This mirrors the logic in your workload crate and ALN grammar at a high level.
pub fn compute_ker_from_workload(risks: WorkloadRiskCoords, residual: ResidualSlice) -> KerTriad {
    let vt = risks.residual();

    let r_clamped = risks.clamped();
    let max_r = r_clamped
        .r_energy
        .max(r_clamped.r_hydraulics.max(r_clamped.r_uncertainty));

    let mut k = 0.95 - 0.4 * max_r;
    if residual.delta_vt > 0.0 {
        k -= 0.25;
    }
    if k < 0.0 {
        k = 0.0;
    }
    if k > 1.0 {
        k = 1.0;
    }

    let mut e = 0.95 - vt;
    if residual.delta_vt > 0.0 {
        e -= 0.3;
    }
    if e < 0.0 {
        e = 0.0;
    }
    if e > 1.0 {
        e = 1.0;
    }

    let mut r_factor = vt + residual.delta_vt.max(0.0);
    if r_factor < 0.0 {
        r_factor = 0.0;
    }
    if r_factor > 1.0 {
        r_factor = 1.0;
    }

    KerTriad { k, e, r: r_factor }.clamped()
}

/// Compute KER triad for AI node slice, emphasizing carbon and biodiversity planes.
pub fn compute_ker_from_ai_node(risks: AiNodeRiskCoords, residual: ResidualSlice) -> KerTriad {
    let vt_ai = risks.residual();

    let r_clamped = risks.clamped();
    let max_r = r_clamped
        .r_carbon
        .max(r_clamped.r_biodiversity)
        .max(r_clamped.r_uncertainty)
        .max(r_clamped.r_energy_compute.max(r_clamped.r_cooling_water));

    let mut k = 0.95 - 0.5 * max_r;
    if residual.delta_vt > 0.0 {
        k -= 0.3;
    }
    if k < 0.0 {
        k = 0.0;
    }
    if k > 1.0 {
        k = 1.0;
    }

    let mut e = 0.95 - vt_ai - 0.3 * (r_clamped.r_carbon + r_clamped.r_biodiversity);
    if residual.delta_vt > 0.0 {
        e -= 0.3;
    }
    if e < 0.0 {
        e = 0.0;
    }
    if e > 1.0 {
        e = 1.0;
    }

    let mut r_factor = vt_ai
        + residual.delta_vt.max(0.0)
        + 0.3 * (r_clamped.r_carbon + r_clamped.r_biodiversity)
        + 0.2 * r_clamped.r_uncertainty;
    if r_factor < 0.0 {
        r_factor = 0.0;
    }
    if r_factor > 1.0 {
        r_factor = 1.0;
    }

    KerTriad { k, e, r: r_factor }.clamped()
}

/// Simple ecosafety decision derived from residual and KER.
///
/// This is the spine-level decision shell used by controllers and schedulers.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub enum SafeDecision {
    /// Residual and KER are acceptable; workload or AI job can proceed.
    Accept,
    /// Residual is marginal; workload should be derated or rescheduled.
    Derate,
    /// Residual / KER indicate high risk; workload must be stopped.
    Stop,
}

/// Decide whether a frame is safe given residual, KER, and lane.
pub fn decide_safe(residual: ResidualSlice, ker: KerTriad, lane: Lane) -> SafeDecision {
    let ker_score = ker.score();

    match lane {
        Lane::Production => {
            if residual.delta_vt > 0.0 || ker_score <= 0.0 {
                SafeDecision::Stop
            } else if ker.e < 0.5 || ker.r > 0.5 {
                SafeDecision::Derate
            } else {
                SafeDecision::Accept
            }
        }
        Lane::Pilot => {
            if ker_score <= 0.0 || ker.r > 0.7 {
                SafeDecision::Stop
            } else if residual.delta_vt > 1.0 {
                SafeDecision::Derate
            } else {
                SafeDecision::Accept
            }
        }
        Lane::Research => {
            if ker_score <= 0.0 && ker.r > 0.8 {
                SafeDecision::Stop
            } else {
                SafeDecision::Accept
            }
        }
    }
}

/// Timestamp helper to get current UTC time in ISO-8601.
///
/// Used for constructing timestamputc fields consistently.
pub fn now_utc_iso8601() -> String {
    OffsetDateTime::now_utc()
        .format(&time::format_description::well_known::Rfc3339)
        .unwrap_or_else(|_| "1970-01-01T00:00:00Z".to_string())
}

/// Shredding governance snapshot canonical JSON envelope.
///
/// This is the Rust-side representation of what C++ adapters return.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShreddingSnapshot {
    /// Shredder + screen + KER snapshot.
    pub machine_id: String,
    pub region: String,
    pub lane: String,
    pub ker: KerTriad,
    pub residual: ResidualSlice,
    pub carbon_negative_ok: bool,
    pub restoration_ok: bool,
    pub lane_admissible: bool,
}

/// Pump accountability snapshot canonical JSON envelope.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PumpSnapshot {
    pub asset_id: String,
    pub site_code: String,
    pub region: String,
    pub lane: String,
    pub corridor_status: String,
    pub decision_mode: String,
    pub ker: KerTriad,
    pub residual: ResidualSlice,
    pub window_start_utc: String,
    pub window_end_utc: String,
}

/// Top-level wrapper for AI-facing snapshots surfaced as JSON.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "kind", content = "payload")]
pub enum AiSnapshot {
    /// Shredding governance snapshot.
    Shredding(ShreddingSnapshot),
    /// Pump accountability snapshot.
    Pump(PumpSnapshot),
}

/// Construct an AI snapshot for shredding governance from typed structs.
///
/// C++ adapters can call into this via FFI and get a stable JSON schema
/// for AI-chat tools and MCP surfaces.
pub fn build_shredding_snapshot_json(
    machine_id: String,
    region: String,
    lane: String,
    ker: KerTriad,
    residual: ResidualSlice,
    carbon_negative_ok: bool,
    restoration_ok: bool,
    lane_admissible: bool,
) -> Value {
    let snapshot = AiSnapshot::Shredding(ShreddingSnapshot {
        machine_id,
        region,
        lane,
        ker: ker.clamped(),
        residual,
        carbon_negative_ok,
        restoration_ok,
        lane_admissible,
    });

    serde_json::to_value(snapshot).unwrap_or_else(|_| json!({ "error": "serialization_failed" }))
}

/// Construct an AI snapshot for pump accountability as JSON.
pub fn build_pump_snapshot_json(
    asset_id: String,
    site_code: String,
    region: String,
    lane: String,
    corridor_status: String,
    decision_mode: String,
    ker: KerTriad,
    residual: ResidualSlice,
    window_start_utc: String,
    window_end_utc: String,
) -> Value {
    let snapshot = AiSnapshot::Pump(PumpSnapshot {
        asset_id,
        site_code,
        region,
        lane,
        corridor_status,
        decision_mode,
        ker: ker.clamped(),
        residual,
        window_start_utc,
        window_end_utc,
    });

    serde_json::to_value(snapshot).unwrap_or_else(|_| json!({ "error": "serialization_failed" }))
}

/// Internal helper: convert C string pointer to &str using SpineError.
fn cstr_to_str<'a>(ptr: *const c_char) -> Result<&'a str, SpineError> {
    if ptr.is_null() {
        return Err(SpineError::InvalidArgument("null pointer".into()));
    }
    unsafe { CStr::from_ptr(ptr) }
        .to_str()
        .map_err(|_| SpineError::InvalidArgument("invalid UTF-8".into()))
}

/// Internal helper: serialize any Serialize value to a C string.
fn to_json_cstring<T: Serialize>(value: &T) -> *mut c_char {
    match serde_json::to_string(value) {
        Ok(s) => match CString::new(s) {
            Ok(cstr) => cstr.into_raw(),
            Err(_) => ptr::null_mut(),
        },
        Err(_) => ptr::null_mut(),
    }
}

/// Internal helper: JSON error envelope as C string.
fn error_json_internal(msg: &str) -> *mut c_char {
    let payload = json!({ "error": msg.to_string() }).to_string();
    match CString::new(payload) {
        Ok(cstr) => cstr.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

macro_rules! impl_ffi_query {
    (
        $(#[$meta:meta])*
        fn $name:ident(
            $handle:ident : *mut ShardIndex,
            $( $arg_name:ident : *const c_char ),* $(,)?
        ) -> *mut c_char
        {
            $body:block
        }
    ) => {
        $(#[$meta])*
        #[no_mangle]
        pub extern "C" fn $name(
            $handle: *mut ShardIndex,
            $( $arg_name: *const c_char ),*
        ) -> *mut c_char {
            if $handle.is_null() {
                return error_json_internal("invalid null ShardIndex handle");
            }

            let shard = unsafe { &mut *$handle };

            $(
                let $arg_name = match cstr_to_str($arg_name) {
                    Ok(s) => s.to_owned(),
                    Err(e) => return error_json_internal(&e.to_string()),
                };
            )*

            let result: Result<impl Serialize, SpineError> = (|| $body)();

            match result {
                Ok(value) => to_json_cstring(&value),
                Err(e) => error_json_internal(&e.to_string()),
            }
        }
    };
}

/// Shredding KER + lane snapshot as JSON for governance adapters.
///
/// This FFI entry stays strictly non-actuating and reads from the
/// EcoNet governance spine via ShardIndex.
#[repr(C)]
#[derive(Debug, Serialize)]
pub struct ShreddingKerSnapshotJson {
    /// Machine identifier (shredder / node id).
    pub machine_id: String,
    /// Region / basin.
    pub region: String,
    /// Lane string (RESEARCH / PILOT / PRODUCTION).
    pub lane: String,
    /// Raw carbon radius.
    pub carbon_radius: f64,
    /// Raw biodiversity radius.
    pub biodiversity_radius: f64,
    /// KER-weighted carbon radius.
    pub ker_weighted_carbon_radius: f64,
    /// KER-weighted biodiversity radius.
    pub ker_weighted_biodiversity_radius: f64,
    /// Knowledge factor.
    pub k_score: f64,
    /// Eco-impact factor.
    pub e_score: f64,
    /// Risk-of-harm factor.
    pub r_score: f64,
    /// Lyapunov residual.
    pub vt_residual: f64,
    /// Rule-of-harm scalar.
    pub roh_scalar: f64,
    /// Lane carbon-negative gate.
    pub carbon_negative_ok: bool,
    /// Lane restoration gate.
    pub restoration_ok: bool,
    /// Overall lane admissible flag.
    pub lane_admissible: bool,
    /// KER predicate OK for lane.
    pub lane_ker_ok: bool,
    /// Cyboquatic predicate OK for lane.
    pub lane_cyboquatic_ok: bool,
    /// Derived production safety flag.
    pub shredding_safe_for_prod: bool,
    /// Derived restoration-focus flag.
    pub shredding_requires_restoration_focus: bool,
    /// Human-readable lane reason.
    pub lane_reason: String,
}

fn build_shredding_snapshot(
    ker: KerBlastRadiusSnapshot,
    lane: LaneAdmissibilityVerdict,
) -> ShreddingKerSnapshotJson {
    let carbon_negative_ok = lane.carbonnegativeok;
    let restoration_ok = lane.restorationok;

    let lane_admissible = lane.admissible && carbon_negative_ok && restoration_ok;
    let lane_ker_ok = lane.kok && lane.eok && lane.rok && lane.rohok;
    let lane_cyboquatic_ok = lane.cyboquaticok;

    let shredding_safe_for_prod = lane_admissible && lane_ker_ok && lane_cyboquatic_ok;
    let shredding_requires_restoration_focus =
        (!restoration_ok && lane.admissible) || (restoration_ok && !carbon_negative_ok);

    ShreddingKerSnapshotJson {
        machine_id: ker.machine_id.clone(),
        region: ker.region.clone(),
        lane: ker.lane.clone(),
        carbon_radius: ker.carbon_radius,
        biodiversity_radius: ker.biodiversity_radius,
        ker_weighted_carbon_radius: ker.ker_weighted_carbon_radius,
        ker_weighted_biodiversity_radius: ker.ker_weighted_biodiversity_radius,
        k_score: ker.kscore,
        e_score: ker.escore,
        r_score: ker.rscore,
        vt_residual: ker.vt_residual,
        roh_scalar: ker.roh_scalar,
        carbon_negative_ok,
        restoration_ok,
        lane_admissible,
        lane_ker_ok,
        lane_cyboquatic_ok,
        shredding_safe_for_prod,
        shredding_requires_restoration_focus,
        lane_reason: lane.reason.clone(),
    }
}

impl_ffi_query! {
    /// FFI entrypoint: return shredding governance snapshot JSON for a machine id.
    fn prometheus_praxis_get_shredding_snapshot_json(
        handle: *mut ShardIndex,
        machine_id: *const c_char,
    ) -> *mut c_char {
        {
            let ker = econet_governance_spine::blastradius::fetch_ker_snapshot_for_machine(
                &shard.conn,
                &machine_id,
            )?;

            let lane = lane_check_for_machine(&shard.conn, &machine_id)?;

            Ok(build_shredding_snapshot(ker, lane))
        }
    }
}

/// Free a JSON string returned from FFI entrypoints.
#[no_mangle]
pub extern "C" fn prometheus_praxis_free_json(ptr_: *mut c_char) {
    if ptr_.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(ptr_);
    }
}

/// FFI shim entrypoint for legacy C++ shredding governance adapter.
///
/// Currently returns a null pointer; pointer dereference must live in
/// a separate module that can use unsafe code if this entrypoint is
/// wired for production.
#[no_mangle]
pub extern "C" fn ppx_ai_get_shredding_snapshot_json(
    machine_id: *const c_char,
    region: *const c_char,
    lane: *const c_char,
    ker_k: f64,
    ker_e: f64,
    ker_r: f64,
    vt_before: f64,
    vt_after: f64,
    carbon_negative_ok: bool,
    restoration_ok: bool,
    lane_admissible: bool,
) -> *mut c_char {
    let _ = (
        machine_id,
        region,
        lane,
        ker_k,
        ker_e,
        ker_r,
        vt_before,
        vt_after,
        carbon_negative_ok,
        restoration_ok,
        lane_admissible,
    );
    ptr::null_mut()
}

/// Kani harnesses for KER and ecosafety decisions.
///
/// These harnesses are compile-time only and never ship in production.
#[cfg(kani)]
mod kani_harnesses {
    use super::*;

    #[kani::proof]
    fn ker_triad_clamped_in_unit_interval() {
        let triad = KerTriad { k: 1.5, e: -0.1, r: 2.3 }.clamped();
        kani::assert!(triad.k >= 0.0 && triad.k <= 1.0);
        kani::assert!(triad.e >= 0.0 && triad.e <= 1.0);
        kani::assert!(triad.r >= 0.0 && triad.r <= 1.0);
    }

    #[kani::proof]
    fn residual_monotone_production_lane() {
        let residual = ResidualSlice::new(0.3, 0.2);
        kani::assert!(residual.is_monotone_for_lane(Lane::Production, 0.0));
    }

    #[kani::proof]
    fn decide_safe_production_stops_on_positive_delta_vt() {
        let residual = ResidualSlice::new(0.2, 0.4);
        let ker = KerTriad { k: 0.9, e: 0.9, r: 0.1 }.clamped();
        let decision = decide_safe(residual, ker, Lane::Production);
        match decision {
            SafeDecision::Stop => {}
            _ => kani::assert!(false),
        }
    }
}
