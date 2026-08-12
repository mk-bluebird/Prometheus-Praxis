// File: crates/prometheus-praxis-ai/src/lib.rs
#![forbid(unsafe_code)]
#![doc = "Prometheus-Praxis non-actuating AI-workload governance primitives."]

pub mod lanes {
    pub mod sdk;
}

pub use lanes::sdk::{
    decide_lane, ActionLane, GovernanceGateConfig, KerCoordinates, LaneAction,
    LaneConfig, LaneDecision, LyapunovResidual, MachineTelemetry, TelemetryDomain,
};

/// Returns a stable schema identifier for telemetry adapters and storage bindings.
pub const WORKLOAD_SCHEMA_ID: &str = "ppx.ai_workload.lane.v1";

/// Produces a compact, stable action label for SQLite, Lua, C++, and device adapters.
pub fn lane_action_code(action: &LaneAction) -> &'static str {
    match action {
        LaneAction::Proceed => "PROCEED",
        LaneAction::Derate => "DERATE",
        LaneAction::Halt => "HALT",
    }
}

/// Produces a compact, stable lane label for cross-language telemetry records.
pub fn action_lane_code(lane: ActionLane) -> &'static str {
    match lane {
        ActionLane::Research => "RESEARCH",
        ActionLane::Pilot => "PILOT",
        ActionLane::Production => "PRODUCTION",
    }
}
