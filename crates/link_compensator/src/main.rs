#![forbid(unsafe_code)]

use anyhow::Result;
use rusqlite::Connection;
use std::env;

mod heuristics;
use heuristics::apply_heuristics;
use link_compensator::{classify_source_kind, eco_impact_for_source, insert_resolution};

fn main() -> Result<()> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: linkcompensator <URL>");
        std::process::exit(1);
    }

    let url = &args[1];
    let source_kind = classify_source_kind(url);
    let eco_score = eco_impact_for_source(&source_kind);

    // TODO: in a follow-up, attempt direct HTTP fetch and Wayback; here we go straight to heuristics.
    let h = apply_heuristics(url, &source_kind);

    // Connect to the EcoRestoration shard SQLite (same DB as other tools).
    let conn = Connection::open("eco_restoration_shard.db")?;

    let result = insert_resolution(
        &conn,
        url,
        &h.fetched_status,
        &source_kind,
        h.archive_url,
        h.snapshot_ts_utc,
        eco_score,
        h.energy_saved_kwh,
        h.co2_offset_kg,
        h.material_recyclability,
        h.data,
    )?;

    // Emit a compact JSON report for AI/CI surfaces.
    let json = serde_json::to_string_pretty(&result)?;
    println!("{json}");

    Ok(())
}
