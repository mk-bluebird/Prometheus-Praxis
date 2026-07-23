// path: crates/prometheus_praxis_ai/src/always_improve.rs
// role: Non-actuating scoring kernel for "always-improve" ranking of
//       diagnostic shards and nodes, plus KER/Lyapunov scoring.
// edition: 2024, rust-version = "1.85"

#![forbid(unsafe_code)]

use crate::{
    AiNodeFrame, DrainageFrame, Lane, ResidualSlice, SafeDecision, WorkloadFrame,
    compute_ker_from_ai_node, compute_ker_from_workload,
};

/// Result of applying the always-improve kernel to a single frame.
#[derive(Debug, Clone)]
pub struct AlwaysImproveResult {
    pub lane_after: Lane,
    pub decision: SafeDecision,
    pub rationale: String,
}

/// High-level classification of corridor status for a frame.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CorridorStatus {
    Safe,
    Marginal,
    Violated,
}

/// Configuration for lane-level always-improve thresholds.
#[derive(Debug, Clone, Copy)]
pub struct LaneImproveConfig {
    pub pilot_delta_vt_eps: f64,
    pub min_k_for_pilot: f64,
    pub min_e_for_production: f64,
    pub max_r_for_production: f64,
    pub max_r_carbon_production: f64,
    pub max_r_biodiversity_production: f64,
}

impl Default for LaneImproveConfig {
    fn default() -> Self {
        LaneImproveConfig {
            pilot_delta_vt_eps: 1.0,
            min_k_for_pilot: 0.6,
            min_e_for_production: 0.7,
            max_r_for_production: 0.4,
            max_r_carbon_production: 0.3,
            max_r_biodiversity_production: 0.3,
        }
    }
}

pub fn corridor_status_drainage(frame: &DrainageFrame) -> CorridorStatus {
    let r = frame.risks.clamped();
    let vt = frame.residual.vt_after;

    let hydraulics_ok = r.r_hydraulics <= 0.3;
    let uncertainty_ok = r.r_uncertainty <= 0.5;
    let residual_ok = vt <= 3.0;

    let hydraulics_warn = r.r_hydraulics <= 0.6;
    let uncertainty_warn = r.r_uncertainty <= 0.7;
    let residual_warn = vt <= 5.0;

    if hydraulics_ok && uncertainty_ok && residual_ok {
        CorridorStatus::Safe
    } else if hydraulics_warn && uncertainty_warn && residual_warn {
        CorridorStatus::Marginal
    } else {
        CorridorStatus::Violated
    }
}

pub fn corridor_status_workload(frame: &WorkloadFrame) -> CorridorStatus {
    let r = frame.risks.clamped();
    let vt = frame.residual.vt_after;

    let energy_ok = r.r_energy <= 0.4;
    let hydraulics_ok = r.r_hydraulics <= 0.3;
    let uncertainty_ok = r.r_uncertainty <= 0.5;
    let residual_ok = vt <= 3.0;

    let energy_warn = r.r_energy <= 0.7;
    let hydraulics_warn = r.r_hydraulics <= 0.6;
    let uncertainty_warn = r.r_uncertainty <= 0.7;
    let residual_warn = vt <= 5.0;

    if energy_ok && hydraulics_ok && uncertainty_ok && residual_ok {
        CorridorStatus::Safe
    } else if energy_warn && hydraulics_warn && uncertainty_warn && residual_warn {
        CorridorStatus::Marginal
    } else {
        CorridorStatus::Violated
    }
}

pub fn corridor_status_ai_node(frame: &AiNodeFrame) -> CorridorStatus {
    let r = frame.risks.clamped();
    let vt_ai = frame.residual_ai.vt_after;

    let energy_ok = r.r_energy_compute <= 0.5;
    let cooling_ok = r.r_cooling_water <= 0.5;
    let carbon_ok = r.r_carbon <= 0.3;
    let biodiversity_ok = r.r_biodiversity <= 0.3;
    let uncertainty_ok = r.r_uncertainty <= 0.5;
    let residual_ok = vt_ai <= 3.0;

    let energy_warn = r.r_energy_compute <= 0.7;
    let cooling_warn = r.r_cooling_water <= 0.7;
    let carbon_warn = r.r_carbon <= 0.5;
    let biodiversity_warn = r.r_biodiversity <= 0.5;
    let uncertainty_warn = r.r_uncertainty <= 0.7;
    let residual_warn = vt_ai <= 5.0;

    if energy_ok
        && cooling_ok
        && carbon_ok
        && biodiversity_ok
        && uncertainty_ok
        && residual_ok
    {
        CorridorStatus::Safe
    } else if energy_warn
        && cooling_warn
        && carbon_warn
        && biodiversity_warn
        && uncertainty_warn
        && residual_warn
    {
        CorridorStatus::Marginal
    } else {
        CorridorStatus::Violated
    }
}

pub fn lane_update_workload(
    frame: &WorkloadFrame,
    ker: crate::KerTriad,
    cfg: LaneImproveConfig,
) -> Lane {
    match frame.lane {
        Lane::Research => {
            if ker.k >= cfg.min_k_for_pilot && ker.e > ker.r {
                Lane::Pilot
            } else {
                Lane::Research
            }
        }
        Lane::Pilot => {
            let monotone_ok = frame
                .residual
                .is_monotone_for_lane(Lane::Pilot, cfg.pilot_delta_vt_eps);
            if ker.e >= cfg.min_e_for_production
                && ker.r <= cfg.max_r_for_production
                && monotone_ok
            {
                Lane::Production
            } else if ker.r > 0.8 {
                Lane::Research
            } else {
                Lane::Pilot
            }
        }
        Lane::Production => {
            let monotone_ok = frame.residual.is_monotone_for_lane(Lane::Production, 0.0);
            if !monotone_ok || ker.r > cfg.max_r_for_production {
                Lane::Pilot
            } else {
                Lane::Production
            }
        }
    }
}

pub fn lane_update_ai_node(
    frame: &AiNodeFrame,
    ker: crate::KerTriad,
    cfg: LaneImproveConfig,
) -> Lane {
    let r = frame.risks.clamped();

    match frame.lane {
        Lane::Research => {
            if ker.k >= cfg.min_k_for_pilot
                && ker.e > ker.r
                && r.r_carbon <= cfg.max_r_carbon_production
                && r.r_biodiversity <= cfg.max_r_biodiversity_production
            {
                Lane::Pilot
            } else {
                Lane::Research
            }
        }
        Lane::Pilot => {
            let monotone_ok = frame
                .residual_ai
                .is_monotone_for_lane(Lane::Pilot, cfg.pilot_delta_vt_eps);
            if ker.e >= cfg.min_e_for_production
                && ker.r <= cfg.max_r_for_production
                && monotone_ok
                && r.r_carbon <= cfg.max_r_carbon_production
                && r.r_biodiversity <= cfg.max_r_biodiversity_production
            {
                Lane::Production
            } else if ker.r > 0.8 || r.r_carbon > 0.7 || r.r_biodiversity > 0.7 {
                Lane::Research
            } else {
                Lane::Pilot
            }
        }
        Lane::Production => {
            let monotone_ok = frame
                .residual_ai
                .is_monotone_for_lane(Lane::Production, 0.0);
            if !monotone_ok
                || ker.r > cfg.max_r_for_production
                || r.r_carbon > cfg.max_r_carbon_production
                || r.r_biodiversity > cfg.max_r_biodiversity_production
            {
                Lane::Pilot
            } else {
                Lane::Production
            }
        }
    }
}

pub fn always_improve_drainage(frame: &DrainageFrame) -> AlwaysImproveResult {
    let status = corridor_status_drainage(frame);
    let lane = Lane::Production;
    let decision = match status {
        CorridorStatus::Safe => SafeDecision::Accept,
        CorridorStatus::Marginal => SafeDecision::Derate,
        CorridorStatus::Violated => SafeDecision::Stop,
    };

    let rationale = match status {
        CorridorStatus::Safe => "Drainage corridor within safe bands; proceed with monitoring.",
        CorridorStatus::Marginal => "Drainage corridor marginal; derate or schedule remediation.",
        CorridorStatus::Violated => "Drainage corridor violated; halt promotions and investigate.",
    }
    .to_string();

    AlwaysImproveResult {
        lane_after: lane,
        decision,
        rationale,
    }
}

pub fn always_improve_workload(
    frame: &WorkloadFrame,
    cfg: LaneImproveConfig,
) -> AlwaysImproveResult {
    let status = corridor_status_workload(frame);
    let ker = compute_ker_from_workload(frame.risks, frame.residual);
    let lane_after = lane_update_workload(frame, ker, cfg);

    let decision = if status == CorridorStatus::Violated {
        SafeDecision::Stop
    } else {
        crate::decide_safe(frame.residual, ker, lane_after)
    };

    let rationale = format!(
        "workload lane_before={:?}, lane_after={:?}, corridor={:?}, k={:.3}, e={:.3}, r={:.3}, kerScore={:.3}, ΔVt={:.3}",
        frame.lane,
        lane_after,
        status,
        ker.k,
        ker.e,
        ker.r,
        ker.score(),
        frame.residual.delta_vt
    );

    AlwaysImproveResult {
        lane_after,
        decision,
        rationale,
    }
}

pub fn always_improve_ai_node(
    frame: &AiNodeFrame,
    cfg: LaneImproveConfig,
) -> AlwaysImproveResult {
    let status = corridor_status_ai_node(frame);
    let ker = compute_ker_from_ai_node(frame.risks, frame.residual_ai);
    let lane_after = lane_update_ai_node(frame, ker, cfg);

    let decision = if status == CorridorStatus::Violated {
        SafeDecision::Stop
    } else {
        crate::decide_safe(frame.residual_ai, ker, lane_after)
    };

    let rationale = format!(
        "ai_node lane_before={:?}, lane_after={:?}, corridor={:?}, k={:.3}, e={:.3}, r={:.3}, kerScore={:.3}, ΔVt_ai={:.3}",
        frame.lane,
        lane_after,
        status,
        ker.k,
        ker.e,
        ker.r,
        ker.score(),
        frame.residual_ai.delta_vt
    );

    AlwaysImproveResult {
        lane_after,
        decision,
        rationale,
    }
}
