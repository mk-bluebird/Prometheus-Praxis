// File: crates/prometheus-praxis-ai/src/lanes/sdk.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
//
// Unified SDK types for telemetry, KER, Lyapunov residuals, and
// lane decisions across Research, Pilot, Production. Non-actuating.
// This module is the frozen grammar layer: structs + validation +
// gate functions. Integration with EcoNet (SQLite, ALN, Phoenix
// hex anchors) happens in adjacent modules, but uses these types
// verbatim.

#![forbid(unsafe_code)]

use rust_decimal::Decimal;

/// Simple domain tag for telemetry sources.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TelemetryDomain {
    WastewaterPump,
    Shredder,
    Hammermill,
    Unknown,
}

/// Unified machine telemetry surface.
/// Specific domains attach domain-specific structs, but lane
/// governance sees this normalized surface.
#[derive(Debug, Clone)]
pub struct MachineTelemetry {
    pub machine_id: String,
    pub station_id: String,
    pub domain: TelemetryDomain,
    pub timestamp_utc: String,

    // Normalized risk coordinates (0..1) coming from numeric kernels.
    pub r_hydraulics: Decimal,
    pub r_energy: Decimal,
    pub r_uncertainty: Decimal,
    pub r_reliability: Decimal,

    // Optional additional planes (e.g., solids, sediment, scum).
    pub r_extra_1: Option<Decimal>,
    pub r_extra_2: Option<Decimal>,

    // Risk of Harm scalar (0..1).
    pub roh: Decimal,

    // Lyapunov residual snapshot for this window.
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

/// Unified KER coordinates for a window.
/// These are computed from MachineTelemetry by KER kernels.
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

/// Lyapunov residual snapshot: V_next - V_current.
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

/// Operational lanes, driven purely by configuration.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ActionLane {
    Research,
    Pilot,
    Production,
}

/// Lane configuration parameters: thresholds for K/E/R and Lyapunov,
/// plus RoH ceiling.
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
                    _ => "invalid lane config value",
                });
            }
        }

        Ok(())
    }

    /// Select lane-specific thresholds (K_min, E_min, R_max).
    pub fn lane_thresholds(&self) -> (Decimal, Decimal, Decimal) {
        match self.lane {
            ActionLane::Research => (self.k_min_research, self.e_min_research, self.r_max_research),
            ActionLane::Pilot => (self.k_min_pilot, self.e_min_pilot, self.r_max_pilot),
            ActionLane::Production => (self.k_min_prod, self.e_min_prod, self.r_max_prod),
        }
    }
}

/// Governance gate for “no corridor -> no build” and related flags.
/// This holds resolved booleans for fast gating.
#[derive(Debug, Clone)]
pub struct GovernanceGateConfig {
    pub corridor_available: bool,
    pub manual_override_allowed: bool,
    pub allow_research_exploration: bool,
}

/// Lane decision result: non-actuating.
/// Actuation logic lives in separate stacks; this type is what
/// EcoNet logs and SQLite/ALN bind to.
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
/// Steps:
/// 1. Hard gate on corridor availability: if no corridor, Halt.
/// 2. Hard gate on RoH ceiling: if roh >= roh_ceiling_global, Halt.
/// 3. Hard gate on Lyapunov residual: if delta_vt > max_delta_vt, Derate/Halt.
/// 4. K/E/R lane thresholds: if below minimum or above max, Derate.
/// 5. Otherwise, Proceed.
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
