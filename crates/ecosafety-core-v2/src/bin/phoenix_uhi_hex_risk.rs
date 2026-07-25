// filename: crates/ecosafety-core-v2/src/bin/phoenix_uhi_hex_risk.rs
// destination: https://github.com/mk-bluebird/Prometheus-Praxis/crates/ecosafety-core-v2/src/bin/phoenix_uhi_hex_risk.rs
// edition: 2024
// rust-version = "1.85"
// license: MIT OR Apache-2.0

#![forbid(unsafe_code)]

use std::env;
use std::fs::File;
use std::io::{BufReader, Read};
use std::process::ExitCode;

use ecosafety_core_v2::uhi_triads::{build_uhi_triad, apply_uhi_to_riskvector, UhiBands, UhiObservables, UhiTriadRisk};
use ecosafety_core_v2::{RiskVector, Scalar};
use serde::{Deserialize, Serialize};
use serde_json::Value;

/// Input record for a Phoenix hex or tract, matching CSV/JSON telemetry.
/// Minimal fields: hex_id, surface_temp_c, ndvi, canopy_fraction, air_quality_index.
#[derive(Debug, Clone, Deserialize)]
struct HexTelemetry {
    pub hex_id: String,
    pub surface_temp_c: Scalar,
    pub ndvi: Scalar,
    pub canopy_fraction: Option<Scalar>,
    pub air_quality_index: Scalar,
}

/// Output ALN-ready shard structure: risk scalars plus r_thermal.
/// This is non-actuating and can be ingested by your DefinitionRegistry / EcoNet tools.
#[derive(Debug, Clone, Serialize)]
struct HexUhiRiskShard {
    pub shard_kind: String,      // e.g. "phoenix-uhi-hex-risk.v1"
    pub hex_id: String,
    pub r_t: Scalar,
    pub r_c: Scalar,
    pub r_a: Scalar,
    pub r_thermal: Scalar,
    pub r_hyd: Scalar,
    pub r_energy: Scalar,
    pub r_biodiv: Scalar,
    pub r_ai: Scalar,
}

fn read_json_array(path: &str) -> Result<Vec<HexTelemetry>, String> {
    let file = File::open(path).map_err(|e| format!("failed to open {path}: {e}"))?;
    let mut reader = BufReader::new(file);
    let mut buf = String::new();
    reader
        .read_to_string(&mut buf)
        .map_err(|e| format!("failed to read {path}: {e}"))?;

    let v: Value = serde_json::from_str(&buf).map_err(|e| format!("invalid JSON: {e}"))?;
    let arr = v
        .as_array()
        .ok_or_else(|| "expected top-level JSON array".to_string())?;

    let mut out = Vec::new();
    for item in arr {
        let rec: HexTelemetry = serde_json::from_value(item.clone())
            .map_err(|e| format!("invalid hex record: {e}"))?;
        out.push(rec);
    }
    Ok(out)
}

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage:");
        eprintln!("  phoenix_uhi_hex_risk --json path/to/hex_uhi_telemetry.json");
        return ExitCode::from(1);
    }

    let mut json_path: Option<String> = None;

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--json" if i + 1 < args.len() => {
                json_path = Some(args[i + 1].clone());
                i += 2;
            }
            _ => {
                eprintln!("Unknown or incomplete argument: {}", args[i]);
                i += 1;
            }
        }
    }

    let json_path = match json_path {
        Some(p) => p,
        None => {
            eprintln!("Missing --json PATH argument");
            return ExitCode::from(1);
        }
    };

    // Example UHI bands for Phoenix; tune via calibration:
    // - T_min_safe_c: ~30°C baseline summer mean.
    // - T_max_extreme_c: ~45°C extreme heat.[web:35][web:39]
    // - NDVI ranges and canopy targets from Phoenix vegetation / cooling studies.[web:38][web:44][web:46]
    // - AQI ranges: 0..50 good, 200 hazardous.[web:44]
    let bands = UhiBands {
        t_min_safe_c: 30.0,
        t_max_extreme_c: 45.0,
        ndvi_min: 0.1,
        ndvi_optimal: 0.6,
        canopy_min: 0.05,
        canopy_target: 0.30,
        aqi_good: 0.0,
        aqi_hazardous: 200.0,
        w_temp: 0.5,
        w_veg: 0.3,
        w_air: 0.2,
    };

    let telemetry = match read_json_array(&json_path) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("{e}");
            return ExitCode::from(1);
        }
    };

    let mut shards: Vec<HexUhiRiskShard> = Vec::new();

    for rec in telemetry {
        let observed = UhiObservables {
            surface_temp_c: rec.surface_temp_c,
            ndvi: rec.ndvi,
            canopy_fraction: rec.canopy_fraction,
            air_quality_index: rec.air_quality_index,
        };

        let triad: UhiTriadRisk = match build_uhi_triad(&observed, &bands) {
            Ok(t) => t,
            Err(e) => {
                eprintln!("hex {}: UHI triad error: {}", rec.hex_id, e);
                continue;
            }
        };

        // Start from a "empty" RiskVector (all planes zero),
        // then apply UHI r_thermal; other planes can be filled by other crates.[file:14]
        let rv0 = RiskVector::new(0.0, 0.0, 0.0, 0.0, 0.0).expect("risk vector init");
        let rv = apply_uhi_to_riskvector(rv0, &triad);

        let shard = HexUhiRiskShard {
            shard_kind: "phoenix-uhi-hex-risk.v1".to_string(),
            hex_id: rec.hex_id,
            r_t: triad.r_t,
            r_c: triad.r_c,
            r_a: triad.r_a,
            r_thermal: triad.r_thermal,
            r_hyd: rv.r_hyd,
            r_energy: rv.r_energy,
            r_biodiv: rv.r_biodiv,
            r_ai: rv.r_ai,
        };
        shards.push(shard);
    }

    // Emit JSONL to stdout: one shard per line, easy to ingest by ALN/DefinitionRegistry.
    for shard in shards {
        let line = serde_json::to_string(&shard).unwrap_or_else(|_| "{}".to_string());
        println!("{line}");
    }

    ExitCode::from(0)
}
