// File: crates/prometheus_praxis_sdk/src/lanes.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
//
// Unified lane grammar: MachineTelemetry, KerCoordinates,
// LyapunovResidual, LaneConfig, GovernanceGateConfig, and
// LaneDecision, plus the core decide_lane function.
//
// This module is the single source of truth for Research,
// Pilot, Production, and Blocked semantics. Differences
// between lanes arise purely from configuration, not from
// divergent code paths.

use rust_decimal::Decimal;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TelemetryDomain {
    WastewaterPump,
    Shredder,
    Hammermill,
    Conveyance,
    Magnet,
    Unknown,
}

/// Unified machine telemetry surface for lane governance.
///
/// Domain-specific modules (e.g., wastewater, shredding) should
/// compute their own normalized risk coordinates and expose them
/// through this struct, which is then fed into KER and lane logic.
#[derive(Debug, Clone)]
pub struct MachineTelemetry {
    pub machine_id: String,
    pub station_id: String,
    pub domain: TelemetryDomain,
    pub timestamp_utc: String,

    pub r_hydraulics: Decimal,
    pub r_energy: Decimal,
    pub r_uncertainty: Decimal,
    pub r_reliability: Decimal,

    pub r_extra_1: Option<Decimal>,
    pub r_extra_2: Option<Decimal>,

    pub roh: Decimal,

    pub vt_current: Decimal,
    pub vt_next: Decimal,
}

impl MachineTelemetry {
    pub fn validate(&self) -> Result<(), &'static str> {
        fn in01(x: &Decimal) -> bool {
            let zero = Decimal::ZERO;
            let one = Decimal::ONE;
            *x >= zero && *x <= one
        }

        if self.machine_id.is_empty() {
            return Err("machine_id must not be empty");
        }
        if self.station_id.is_empty() {
            return Err("station_id must not be empty");
        }
        if self.timestamp_utc.is_empty() {
            return Err("timestamp_utc must not be empty");
        }

        if !in01(&self.r_hydraulics) {
            return Err("r_hydraulics must be in [0,1]");
        }
        if !in01(&self.r_energy) {
            return Err("r_energy must be in [0,1]");
        }
        if !in01(&self.r_uncertainty) {
            return Err("r_uncertainty must be in [0,1]");
        }
        if !in01(&self.r_reliability) {
            return Err("r_reliability must be in [0,1]");
        }

        if let Some(x) = &self.r_extra_1 {
            if !in01(x) {
                return Err("r_extra_1 must be in [0,1]");
            }
        }
        if let Some(x) = &self.r_extra_2 {
            if !in01(x) {
                return Err("r_extra_2 must be in [0,1]");
            }
        }

        if !in01(&self.roh) {
            return Err("roh must be in [0,1]");
        }

        Ok(())
    }
}

/// KER coordinates: Knowledge, Eco-impact, and Risk.
///
/// These are computed by domain-specific KER kernels, based on
/// MachineTelemetry and additional context. Lane governance only
/// cares that they are normalized into [0,1].
#[derive(Debug, Clone)]
pub struct KerCoordinates {
    pub k_knowledge: Decimal,
    pub e_eco_impact: Decimal,
    pub r_risk: Decimal,
}

impl KerCoordinates {
    pub fn validate(&self) -> Result<(), &'static str> {
        fn in01(x: &Decimal) -> bool {
            let zero = Decimal::ZERO;
            let one = Decimal::ONE;
            *x >= zero && *x <= one
        }

        if !in01(&self.k_knowledge) {
            return Err("k_knowledge must be in [0,1]");
        }
        if !in01(&self.e_eco_impact) {
            return Err("e_eco_impact must be in [0,1]");
        }
        if !in01(&self.r_risk) {
            return Err("r_risk must be in [0,1]");
        }

        Ok(())
    }
}

/// Lyapunov residual snapshot for a single telemetry window.
///
/// vt_current and vt_next are quadratic forms over risk coordinates.
/// delta_vt indicates whether risk is dissipating or accumulating.
#[derive(Debug, Clone)]
pub struct LyapunovResidual {
    pub vt_current: Decimal,
    pub vt_next: Decimal,
    pub delta_vt: Decimal,
}

impl LyapunovResidual {
    pub fn from_telemetry(t: &MachineTelemetry) -> Self {
        let delta = t.vt_next - t.vt_current;
        Self {
            vt_current: t.vt_current,
            vt_next: t.vt_next,
            delta_vt: delta,
        }
    }

    pub fn is_stable(&self, max_allowed_increase: Decimal) -> bool {
        self.delta_vt <= max_allowed_increase
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ActionLane {
    Research,
    Pilot,
    Production,
}

/// Lane configuration parameters. This struct encodes the
/// operational semantics for each lane via thresholds, not
/// via divergent control flow.
#[derive(Debug, Clone)]
pub struct LaneConfig {
    pub lane: ActionLane,

    pub roh_ceiling_global: Decimal,
    pub max_delta_vt: Decimal,

    pub k_min_research: Decimal,
    pub k_min_pilot: Decimal,
    pub k_min_prod: Decimal,

    pub e_min_research: Decimal,
    pub e_min_pilot: Decimal,
    pub e_min_prod: Decimal,

    pub r_max_research: Decimal,
    pub r_max_pilot: Decimal,
    pub r_max_prod: Decimal,
}

impl LaneConfig {
    pub fn validate(&self) -> Result<(), &'static str> {
        fn in01(x: &Decimal) -> bool {
            let zero = Decimal::ZERO;
            let one = Decimal::ONE;
            *x >= zero && *x <= one
        }

        if !in01(&self.roh_ceiling_global) {
            return Err("roh_ceiling_global must be in [0,1]");
        }
        if self.max_delta_vt < Decimal::ZERO {
            return Err("max_delta_vt must be >= 0");
        }

        for (label, v) in [
            ("k_min_research", &self.k_min_research),
            ("k_min_pilot", &self.k_min_pilot),
            ("k_min_prod", &self.k_min_prod),
            ("e_min_research", &self.e_min_research),
            ("e_min_pilot", &self.e_min_pilot),
            ("e_min_prod", &self.e_min_prod),
            ("r_max_research", &self.r_max_research),
            ("r_max_pilot", &self.r_max_pilot),
            ("r_max_prod", &self.r_max_prod),
        ] {
            if !in01(v) {
                return Err(match label {
                    "k_min_research" => "k_min_research must be in [0,1]",
                    "k_min_pilot" => "k_min_pilot must be in [0,1]",
                    "k_min_prod" => "k_min_prod must be in [0,1]",
                    "e_min_research" => "e_min_research must be in [0,1]",
                    "e_min_pilot" => "e_min_pilot must be in [0,1]",
                    "e_min_prod" => "e_min_prod must be in [0,1]",
                    "r_max_research" => "r_max_research must be in [0,1]",
                    "r_max_pilot" => "r_max_pilot must be in [0,1]",
                    "r_max_prod" => "r_max_prod must be in [0,1]",
                    _ => "invalid lane configuration value",
                });
            }
        }

        Ok(())
    }

    pub fn lane_thresholds(&self) -> (Decimal, Decimal, Decimal) {
        match self.lane {
            ActionLane::Research => (self.k_min_research, self.e_min_research, self.r_max_research),
            ActionLane::Pilot => (self.k_min_pilot, self.e_min_pilot, self.r_max_pilot),
            ActionLane::Production => (self.k_min_prod, self.e_min_prod, self.r_max_prod),
        }
    }
}

/// Governance gate flags resolved from EcoNet metadata.
///
/// corridor_available implements "no corridor -> no build".
/// manual_override_allowed and allow_research_exploration allow
/// explicit override semantics where permitted.
#[derive(Debug, Clone)]
pub struct GovernanceGateConfig {
    pub corridor_available: bool,
    pub manual_override_allowed: bool,
    pub allow_research_exploration: bool,
}

#[derive(Debug, Clone)]
pub enum LaneAction {
    Proceed,
    Derate,
    Halt,
}

#[derive(Debug, Clone)]
pub struct LaneDecision {
    pub lane: ActionLane,
    pub action: LaneAction,
    pub reason_code: String,
    pub vt_current: Decimal,
    pub vt_next: Decimal,
    pub roh: Decimal,
    pub k: Decimal,
    pub e: Decimal,
    pub r: Decimal,
}

impl LaneDecision {
    pub fn halt(lane: ActionLane, reason: &str, t: &MachineTelemetry, ker: &KerCoordinates) -> Self {
        Self {
            lane,
            action: LaneAction::Halt,
            reason_code: reason.to_owned(),
            vt_current: t.vt_current,
            vt_next: t.vt_next,
            roh: t.roh,
            k: ker.k_knowledge,
            e: ker.e_eco_impact,
            r: ker.r_risk,
        }
    }

    pub fn derate(lane: ActionLane, reason: &str, t: &MachineTelemetry, ker: &KerCoordinates) -> Self {
        Self {
            lane,
            action: LaneAction::Derate,
            reason_code: reason.to_owned(),
            vt_current: t.vt_current,
            vt_next: t.vt_next,
            roh: t.roh,
            k: ker.k_knowledge,
            e: ker.e_eco_impact,
            r: ker.r_risk,
        }
    }

    pub fn proceed(lane: ActionLane, reason: &str, t: &MachineTelemetry, ker: &KerCoordinates) -> Self {
        Self {
            lane,
            action: LaneAction::Proceed,
            reason_code: reason.to_owned(),
            vt_current: t.vt_current,
            vt_next: t.vt_next,
            roh: t.roh,
            k: ker.k_knowledge,
            e: ker.e_eco_impact,
            r: ker.r_risk,
        }
    }
}

/// Unified governance gate across lanes.
///
/// This function is the core of lane semantics. It does not
/// branch on domain; it only uses configuration and telemetry
/// to decide Proceed, Derate, or Halt.
pub fn decide_lane(
    telemetry: &MachineTelemetry,
    ker: &KerCoordinates,
    lane_cfg: &LaneConfig,
    gate_cfg: &GovernanceGateConfig,
) -> LaneDecision {
    if let Err(e) = telemetry.validate() {
        return LaneDecision::halt(lane_cfg.lane, e, telemetry, ker);
    }
    if let Err(e) = ker.validate() {
        return LaneDecision::halt(lane_cfg.lane, e, telemetry, ker);
    }
    if let Err(e) = lane_cfg.validate() {
        return LaneDecision::halt(lane_cfg.lane, e, telemetry, ker);
    }

    let roh_val = telemetry.roh;
    let lyap = LyapunovResidual::from_telemetry(telemetry);
    let (k_min, e_min, r_max) = lane_cfg.lane_thresholds();
    let k = ker.k_knowledge;
    let e = ker.e_eco_impact;
    let r = ker.r_risk;

    if !gate_cfg.corridor_available {
        return LaneDecision::halt(
            lane_cfg.lane,
            "no_corridor_no_build",
            telemetry,
            ker,
        );
    }

    if roh_val >= lane_cfg.roh_ceiling_global {
        return LaneDecision::halt(
            lane_cfg.lane,
            "roh_exceeds_global_ceiling",
            telemetry,
            ker,
        );
    }

    if lyap.delta_vt > lane_cfg.max_delta_vt {
        if lane_cfg.lane == ActionLane::Research && gate_cfg.allow_research_exploration {
            return LaneDecision::derate(
                lane_cfg.lane,
                "lyapunov_delta_exceeds_research_band",
                telemetry,
                ker,
            );
        }

        let reason = "lyapunov_delta_exceeds_lane_band";
        return match lane_cfg.lane {
            ActionLane::Pilot => LaneDecision::derate(lane_cfg.lane, reason, telemetry, ker),
            ActionLane::Production => LaneDecision::halt(lane_cfg.lane, reason, telemetry, ker),
            ActionLane::Research => LaneDecision::halt(lane_cfg.lane, reason, telemetry, ker),
        };
    }

    if k < k_min {
        return LaneDecision::derate(
            lane_cfg.lane,
            "knowledge_below_lane_min",
            telemetry,
            ker,
        );
    }
    if e < e_min {
        return LaneDecision::derate(
            lane_cfg.lane,
            "eco_impact_below_lane_min",
            telemetry,
            ker,
        );
    }
    if r > r_max {
        return LaneDecision::derate(
            lane_cfg.lane,
            "risk_above_lane_max",
            telemetry,
            ker,
        );
    }

    LaneDecision::proceed(
        lane_cfg.lane,
        "all_governance_gates_passed",
        telemetry,
        ker,
    )
}
