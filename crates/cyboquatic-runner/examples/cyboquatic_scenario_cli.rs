#![forbid(unsafe_code)]

use std::env;
use std::process::exit;

use cyboquatic_runner::run_cyboquatic_scenario;

fn main() {
    let epochs = env::args()
        .nth(1)
        .and_then(|s| s.parse::<usize>().ok())
        .unwrap_or(128);

    let result = run_cyboquatic_scenario(epochs);

    println!(
        "Cyboquatic run: pass_overall={} (residual_stability={}, ker_bounds={}, index_recovery={}, policy_no_panic={}, emergency_triggers={})",
        result.pass_overall,
        result.pass_residual_stability,
        result.pass_ker_bounds,
        result.pass_cybo_index_recovery,
        result.pass_policy_no_panic,
        result.emergency_policy_triggers,
    );

    if !result.pass_overall {
        eprintln!("[CYBO] Cyboquatic scenario FAILED governance criteria.");
        exit(1);
    }
}
