// filename: eco_restoration_shard_config/src/bin/print_config_report.rs

//! Small helper binary to load config.yaml and print a machine-readable report.
//!
//! This is safe for CI usage and for AI-chat tooling that wants a single,
//! stable JSON summary of configuration status.

use std::env;
use std::path::PathBuf;

use eco_restoration_shard_config::{load_config_from_path, DEFAULT_CONFIG_PATH};
use eco_restoration_shard_config::ConfigError;

fn main() {
    let path = env::args()
        .nth(1)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(DEFAULT_CONFIG_PATH));

    match load_config_from_path(&path) {
        Ok(cfg) => {
            let report = serde_json::json!({
                "ok": true,
                "path": path.to_string_lossy(),
                "config": {
                    "version": cfg.version,
                    "owner_did": cfg.owner_did,
                    "providers": cfg.providers.iter().map(|p| {
                        serde_json::json!({
                            "name": p.name,
                            "endpoint": p.endpoint,
                            "apikey_env": p.apikey_env,
                            "lane": p.lane,
                            "non_actuating": p.non_actuating,
                        })
                    }).collect::<Vec<_>>(),
                },
            });
            println!("{}", serde_json::to_string_pretty(&report).unwrap());
        }
        Err(err) => match err {
            ConfigError::Validation(report) => {
                let json = serde_json::json!({
                    "ok": false,
                    "path": path.to_string_lossy(),
                    "error": "validation",
                    "report": report,
                });
                println!("{}", serde_json::to_string_pretty(&json).unwrap());
                std::process::exit(1);
            }
            ConfigError::NotFound(p) => {
                let json = serde_json::json!({
                    "ok": false,
                    "path": p,
                    "error": "not_found",
                });
                println!("{}", serde_json::to_string_pretty(&json).unwrap());
                std::process::exit(1);
            }
            ConfigError::Io { path: p, source } => {
                let json = serde_json::json!({
                    "ok": false,
                    "path": p,
                    "error": "io",
                    "message": source.to_string(),
                });
                println!("{}", serde_json::to_string_pretty(&json).unwrap());
                std::process::exit(1);
            }
            ConfigError::Parse { path: p, source } => {
                let json = serde_json::json!({
                    "ok": false,
                    "path": p,
                    "error": "parse",
                    "message": source.to_string(),
                });
                println!("{}", serde_json::to_string_pretty(&json).unwrap());
                std::process::exit(1);
            }
        },
    }
}
