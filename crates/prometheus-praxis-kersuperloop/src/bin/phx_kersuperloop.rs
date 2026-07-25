// filename: crates/prometheus-praxis-kersuperloop/src/bin/phx_kersuperloop.rs
// destination: https://github.com/mk-bluebird/Prometheus-Praxis/crates/prometheus-praxis-kersuperloop/src/bin/phx_kersuperloop.rs
// edition: 2024
// rust-version = "1.85"
// license: MIT OR Apache-2.0

#![forbid(unsafe_code)]

use std::env;
use std::process::ExitCode;

use serde_json::json;

use prometheus_praxis_kersuperloop::{Scalar};
use prometheus_praxis_kersuperloop::phoenix_spine::evaluate_phoenix_node_from_spine;

/// Thin CLI that emits JSON KER-Lyapunov diagnostics for Phoenix nodes.[file:14][file:13]
///
/// Usage:
///   phx-kersuperloop --db /path/to/cyboquatic.db --node NODEID --region Phoenix-AZ --k 0.96 --e 0.93 --r 0.11
///
/// Output:
///   JSON to stdout, either a PhoenixNodeDiagnostic object or `null` if
///   no window is recorded for the given node/region.
fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    let mut db_path: Option<String> = None;
    let mut node_id: Option<String> = None;
    let mut region: Option<String> = None;
    let mut k: Option<Scalar> = None;
    let mut e: Option<Scalar> = None;
    let mut r: Option<Scalar> = None;

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--db" if i + 1 < args.len() => {
                db_path = Some(args[i + 1].clone());
                i += 2;
            }
            "--node" if i + 1 < args.len() => {
                node_id = Some(args[i + 1].clone());
                i += 2;
            }
            "--region" if i + 1 < args.len() => {
                region = Some(args[i + 1].clone());
                i += 2;
            }
            "--k" if i + 1 < args.len() => {
                k = args[i + 1].parse::<Scalar>().ok();
                i += 2;
            }
            "--e" if i + 1 < args.len() => {
                e = args[i + 1].parse::<Scalar>().ok();
                i += 2;
            }
            "--r" if i + 1 < args.len() => {
                r = args[i + 1].parse::<Scalar>().ok();
                i += 2;
            }
            _ => {
                eprintln!("Unknown or incomplete argument: {}", args[i]);
                i += 1;
            }
        }
    }

    if db_path.is_none() || node_id.is_none() || region.is_none() || k.is_none() || e.is_none() || r.is_none() {
        eprintln!("Usage:");
        eprintln!("  phx-kersuperloop --db PATH --node NODEID --region REGION --k K --e E --r R");
        return ExitCode::from(1);
    }

    let db_path = db_path.unwrap();
    let node_id = node_id.unwrap();
    let region = region.unwrap();
    let k = k.unwrap();
    let e = e.unwrap();
    let r = r.unwrap();

    match evaluate_phoenix_node_from_spine(&db_path, &node_id, &region, k, e, r) {
        Ok(Some(diag)) => {
            let out = serde_json::to_string_pretty(&diag).unwrap_or_else(|_| "null".to_string());
            println!("{out}");
            ExitCode::from(0)
        }
        Ok(None) => {
            // No window rows for this node/region → emit JSON null.
            println!("null");
            ExitCode::from(0)
        }
        Err(err) => {
            let err_json = json!({
                "error": format!("{err}"),
                "node_id": node_id,
                "region": region,
            });
            println!("{}", serde_json::to_string_pretty(&err_json).unwrap());
            ExitCode::from(1)
        }
    }
}
