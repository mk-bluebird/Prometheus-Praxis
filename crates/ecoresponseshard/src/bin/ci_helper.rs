// FILE: crates/ecoresponseshard/src/bin/ci_helper.rs
// DESTINATION: crates/ecoresponseshard/src/bin/ci_helper.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard
//
// Standalone CI helper: opens the response DB, runs the high-trust
// view query, prints a JSON summary to stdout, and exits non-zero
// if no PROD+HIGH rows are found or if mean D_combined < 0.70.

#![forbid(unsafe_code)]

use std::env;
use std::process::ExitCode;

use ecoresponseshard::{query_prod_high_trust, ProdHighTrustFilter};

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: eco_response_ci <response_db_path> [region]");
        return ExitCode::from(1);
    }

    let db_path = &args[1];
    let region = args.get(2).cloned();

    let filter = ProdHighTrustFilter {
        region,
        limit: 1024,
    };

    let rows = match query_prod_high_trust(db_path, filter) {
        Ok(r) => r,
        Err(e) => {
            eprintln!("query failed: {e}");
            return ExitCode::from(1);
        }
    };

    if rows.is_empty() {
        eprintln!("CI FAIL: no PROD+HIGH trust rows found");
        return ExitCode::from(2);
    }

    let d_mean = rows.iter().map(|r| r.d_combined).sum::<f64>() / rows.len() as f64;
    let k_mean = rows.iter().map(|r| r.k_factor).sum::<f64>() / rows.len() as f64;
    let e_mean = rows.iter().map(|r| r.e_factor).sum::<f64>() / rows.len() as f64;
    let r_mean = rows.iter().map(|r| r.r_factor).sum::<f64>() / rows.len() as f64;

    println!(
        "{{\"row_count\":{},\"k_mean\":{:.4},\"e_mean\":{:.4},\"r_mean\":{:.4},\"d_mean\":{:.4}}}",
        rows.len(),
        k_mean,
        e_mean,
        r_mean,
        d_mean
    );

    const D_FLOOR: f64 = 0.70;
    if d_mean < D_FLOOR {
        eprintln!("CI FAIL: d_mean={d_mean:.4} < floor={D_FLOOR}");
        return ExitCode::from(3);
    }

    ExitCode::SUCCESS
}
