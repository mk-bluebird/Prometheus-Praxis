// filename: src/bin/lane_governance_check.rs
// destination: eco_restoration_shard/src/bin/lane_governance_check.rs

use std::{fs, process::exit};

use eco_restoration_shard::lanes::{LaneGate, LaneKind, LaneStatus};
use eco_restoration_shard::lane_checks::{check_crate_gates, check_hydrology, check_sensor_health, check_topology};
use eco_restoration_shard::lane_config::load_lane_config;

fn main() {
    let crate_name = std::env::var("CARGO_PKG_NAME").unwrap_or_else(|_| "unknown_crate".to_string());

    let lane_status_path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "target/lane_status.json".to_string());

    let data = fs::read_to_string(&lane_status_path).unwrap_or_else(|e| {
        eprintln!("Failed to read {}: {}", lane_status_path, e);
        exit(1);
    });

    let v: serde_json::Value = serde_json::from_str(&data).unwrap_or_else(|e| {
        eprintln!("Failed to parse {} as JSON: {}", lane_status_path, e);
        exit(1);
    });

    let lane = match v.get("lane").and_then(|x| x.as_str()).unwrap_or("Research") {
        "Research" | "research" => LaneKind::Research,
        "ExpProd" | "EXPPROD" | "exp_prod" => LaneKind::ExpProd,
        "Prod" | "PROD" | "production" => LaneKind::Prod,
        _ => LaneKind::Research,
    };

    let status = LaneStatus {
        lane,
        b_slope: v.get("b_slope").and_then(|x| x.as_f64()).unwrap_or(0.0),
        k_band_ok: v.get("k_band_ok").and_then(|x| x.as_bool()).unwrap_or(false),
        e_band_ok: v.get("e_band_ok").and_then(|x| x.as_bool()).unwrap_or(false),
        r_band_ok: v.get("r_band_ok").and_then(|x| x.as_bool()).unwrap_or(false),
    };

    let gate = LaneGate::new();
    if let Err(violations) = gate.check(&status) {
        eprintln!("Lane governance violation(s): {:?}", violations);
        exit(1);
    }

    let config = load_lane_config().unwrap_or_else(|e| {
        eprintln!("Failed to load lane configuration: {}", e);
        exit(1);
    });

    let crate_cfg = match config.get(&crate_name) {
        Some(cfg) => cfg,
        None => {
            eprintln!("Missing lane configuration for crate: {}", crate_name);
            exit(1);
        }
    };

    let hydrology_ok = match check_hydrology(&crate_name) {
        Ok(ok) => ok,
        Err(e) => {
            eprintln!("Hydrology check failed for {}: {}", crate_name, e);
            exit(1);
        }
    };

    let sensor_ok = match check_sensor_health(&crate_name) {
        Ok(ok) => ok,
        Err(e) => {
            eprintln!("Sensor health check failed for {}: {}", crate_name, e);
            exit(1);
        }
    };

    let topology_ok = match check_topology(&crate_name) {
        Ok(ok) => ok,
        Err(e) => {
            eprintln!("Topology check failed for {}: {}", crate_name, e);
            exit(1);
        }
    };

    if let Err(violations) = check_crate_gates(
        &crate_name,
        &status,
        crate_cfg,
        hydrology_ok,
        sensor_ok,
        topology_ok,
    ) {
        eprintln!("Crate {} failed lane governance: {:?}", crate_name, violations);
        exit(1);
    }
}
