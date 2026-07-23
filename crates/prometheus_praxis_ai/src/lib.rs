// filename: crates/prometheus_praxis_ai/src/lib.rs

#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]

use serde::{Deserialize, Serialize};
use time::OffsetDateTime;

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
    /// Compute kerScore = k * (e - r), consistent with ALN ker-axis.
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

/// Hydraulics / drainage frame as governed by DrainageDecayKernel2026v1.aln2.
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

/// Workload frame as governed by WorkloadKernel2026v1.aln2.
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

/// AI datacenter node frame as governed by AiDatacenterNode2026v1.aln2.
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
    let r = risks.clamped();
    let vt = risks.residual();

    let max_r = r.r_energy.max(r.r_hydraulics.max(r.r_uncertainty));

    // Knowledge: penalize high max_r and positive ΔVt.
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

    // Eco-impact: high when vt is small and ΔVt <= 0.
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

    // Risk-of-harm: baseline vt plus positive ΔVt.
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
    let r = risks.clamped();
    let vt_ai = risks.residual();

    // Max risk emphasises carbon, biodiversity, and uncertainty.
    let max_r = r.r_carbon
        .max(r.r_biodiversity)
        .max(r.r_uncertainty)
        .max(r.r_energy_compute.max(r.r_cooling_water));

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

    let mut e = 0.95 - vt_ai - 0.3 * (r.r_carbon + r.r_biodiversity);
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
        + 0.3 * (r.r_carbon + r.r_biodiversity)
        + 0.2 * r.r_uncertainty;
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
    OffsetDateTime::now_utc().format(&time::format_description::well_known::Rfc3339).unwrap_or_else(|_| "1970-01-01T00:00:00Z".to_string())
}
