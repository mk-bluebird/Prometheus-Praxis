// File: crates/prometheus-praxis-ai/src/bin/ppx_ai_lane_check.rs
#![forbid(unsafe_code)]

use prometheus_praxis_ai::{
    action_lane_code, decide_lane, lane_action_code, ActionLane, GovernanceGateConfig,
    KerCoordinates, LaneConfig, MachineTelemetry, TelemetryDomain,
};
use rust_decimal::Decimal;
use std::env;
use std::process::ExitCode;

fn decimal(value: &str, field: &str) -> Result<Decimal, String> {
    value.parse::<Decimal>()
        .map_err(|_| format!("{field} must be a decimal value"))
}

fn lane(value: &str) -> Result<ActionLane, String> {
    match value {
        "research" => Ok(ActionLane::Research),
        "pilot" => Ok(ActionLane::Pilot),
        "production" => Ok(ActionLane::Production),
        _ => Err("lane must be research, pilot, or production".to_owned()),
    }
}

fn main() -> ExitCode {
    let arguments: Vec<String> = env::args().collect();
    if arguments.len() != 12 {
        eprintln!(
            "Usage: ppx-ai-lane-check MACHINE_ID STATION_ID TIMESTAMP_UTC \
LANE R_HYD R_ENERGY R_UNCERTAINTY R_RELIABILITY ROH VT_CURRENT VT_NEXT"
        );
        return ExitCode::from(64);
    }

    let run = || -> Result<(), String> {
        let chosen_lane = lane(&arguments[4])?;
        let telemetry = MachineTelemetry {
            machine_id: arguments[1].clone(),
            station_id: arguments[2].clone(),
            domain: TelemetryDomain::Unknown,
            timestamp_utc: arguments[3].clone(),
            r_hydraulics: decimal(&arguments[5], "r_hydraulics")?,
            r_energy: decimal(&arguments[6], "r_energy")?,
            r_uncertainty: decimal(&arguments[7], "r_uncertainty")?,
            r_reliability: decimal(&arguments[8], "r_reliability")?,
            r_extra_1: None,
            r_extra_2: None,
            roh: decimal(&arguments[9], "roh")?,
            vt_current: decimal(&arguments[10], "vt_current")?,
            vt_next: decimal(&arguments[11], "vt_next")?,
        };

        let ker = KerCoordinates {
            k_knowledge: Decimal::new(80, 2),
            e_eco_impact: Decimal::new(80, 2),
            r_risk: telemetry.r_energy.max(telemetry.r_hydraulics),
        };

        let config = LaneConfig {
            lane: chosen_lane,
            roh_ceiling_global: Decimal::new(25, 2),
            max_delta_vt: Decimal::new(2, 2),
            k_min_research: Decimal::new(50, 2),
            k_min_pilot: Decimal::new(65, 2),
            k_min_prod: Decimal::new(80, 2),
            e_min_research: Decimal::new(50, 2),
            e_min_pilot: Decimal::new(65, 2),
            e_min_prod: Decimal::new(80, 2),
            r_max_research: Decimal::new(35, 2),
            r_max_pilot: Decimal::new(25, 2),
            r_max_prod: Decimal::new(15, 2),
        };

        let decision = decide_lane(
            &telemetry,
            &ker,
            &config,
            &GovernanceGateConfig {
                corridor_available: true,
                manual_override_allowed: false,
                allow_research_exploration: true,
            },
        );

        println!(
            "schema={}\tlane={}\taction={}\treason={}\tdelta_vt={}\troh={}\tk={}\te={}\tr={}",
            prometheus_praxis_ai::WORKLOAD_SCHEMA_ID,
            action_lane_code(decision.lane),
            lane_action_code(&decision.action),
            decision.reason_code,
            decision.vt_next - decision.vt_current,
            decision.roh,
            decision.k,
            decision.e,
            decision.r,
        );
        Ok(())
    };

    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("{error}");
            ExitCode::from(65)
        }
    }
}
