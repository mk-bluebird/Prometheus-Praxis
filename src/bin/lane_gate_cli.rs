use eco_restoration_shard::lanes::{LaneGate, LaneKind, LaneStatus};
use std::{fs, process::exit};

fn main() {
    let path = std::env::args()
        .nth(1)
        .unwrap_or("target/lane_status.json".into());
    let data = fs::read_to_string(path).expect("read lane_status.json");
    let v: serde_json::Value = serde_json::from_str(&data).expect("parse lane_status.json");

    let lane = match v["lane"].as_str().unwrap_or("Research") {
        "Research" => LaneKind::Research,
        "ExpProd" | "EXPPROD" => LaneKind::ExpProd,
        "Prod" | "PROD" => LaneKind::Prod,
        _ => LaneKind::Research,
    };

    let status = LaneStatus {
        lane,
        b_slope: v["b_slope"].as_f64().unwrap_or(0.0),
        k_band_ok: v["k_band_ok"].as_bool().unwrap_or(false),
        e_band_ok: v["e_band_ok"].as_bool().unwrap_or(false),
        r_band_ok: v["r_band_ok"].as_bool().unwrap_or(false),
    };

    let gate = LaneGate::new();
    if let Err(violations) = gate.check(&status) {
        eprintln!("Lane governance violation(s): {:?}", violations);
        exit(1);
    }
}
