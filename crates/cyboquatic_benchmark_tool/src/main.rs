// Filename: crates/cyboquatic_benchmark_tool/src/main.rs
// Destination: crates/cyboquatic_benchmark_tool/src/main.rs

#![forbid(unsafe_code)]

use std::env;
use std::path::PathBuf;

use econet_governance_spine::GovernanceSpine;
use econet_governance_spine::SpineError;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: cyboquatic_benchmark_tool <governance_db_path>");
        std::process::exit(1);
    }

    let db_path = PathBuf::from(args[1].clone());
    let expected = econet_governance_spine::load_expected_schema();
    let spine = match GovernanceSpine::open(&db_path, expected) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("failed to open spine: {}", e);
            std::process::exit(1);
        }
    };

    match run_benchmark(&spine) {
        Ok(()) => {}
        Err(e) => {
            eprintln!("benchmark failed: {}", e);
            std::process::exit(1);
        }
    }
}

fn run_benchmark(spine: &GovernanceSpine) -> Result<(), SpineError> {
    let conn = spine
        .verify_schema()
        .and_then(|_| Ok(spine))
        .expect("schema verified")
        .conn
        .try_clone()
        .expect("clone connection");

    let mut stmt = conn.prepare(
        "SELECT node_id,
                shard_id,
                expected_carbonnegativeok,
                expected_restorationok
         FROM cyboquatic_benchmark",
    )?;
    let mut rows = stmt.query([])?;
    while let Some(row) = rows.next()? {
        let node_id: String = row.get(0)?;
        let shard_id: String = row.get(1)?;
        let expected_carbon: i64 = row.get(2)?;
        let expected_restore: i64 = row.get(3)?;

        let metrics = spine.get_cyboquatic_metrics(&node_id)?;
        let lane = spine.get_lane_status(&shard_id)?;

        let carbon_ok = metrics.carbon_negative_ok;
        let restoration_ok = metrics.restoration_ok;

        if carbon_ok != (expected_carbon != 0) {
            return Err(SpineError::SchemaMismatch(format!(
                "node '{}' carbonnegativeok mismatch: expected {}, got {}",
                node_id, expected_carbon, carbon_ok as i64
            )));
        }

        if restoration_ok != (expected_restore != 0) {
            return Err(SpineError::SchemaMismatch(format!(
                "node '{}' restorationok mismatch: expected {}, got {}",
                node_id, expected_restore, restoration_ok as i64
            )));
        }

        if lane.carbon_negative_ok != carbon_ok || lane.restoration_ok != restoration_ok {
            return Err(SpineError::SchemaMismatch(format!(
                "lane flags mismatch for shard '{}'",
                shard_id
            )));
        }
    }

    Ok(())
}
