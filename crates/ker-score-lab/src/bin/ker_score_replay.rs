// FILE: crates/ker-score-lab/src/bin/ker_score_replay.rs
// DESTINATION: crates/ker-score-lab/src/bin/ker_score_replay.rs
// REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard
//
// CLI: replay the canonical synthetic fixture, print JSON score to stdout,
// persist to a DB file, and exit non-zero if corridor_ok is false.
//
// Usage: ker_score_replay <score_db_path> [lane] [owner_did]

#![forbid(unsafe_code)]

use std::env;
use std::process::ExitCode;

use ker_score_lab::{
    canonical_synthetic_fixture, persist_score_shard, replay_and_score,
};

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!(
            "usage: ker_score_replay <score_db_path> [lane] [owner_did]"
        );
        return ExitCode::from(1);
    }

    let db_path   = &args[1];
    let lane      = args.get(2).map(String::as_str).unwrap_or("RESEARCH");
    let owner_did = args
        .get(3)
        .map(String::as_str)
        .unwrap_or("bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7");

    let fixture = canonical_synthetic_fixture();

    let shard = match replay_and_score(&fixture, lane, owner_did) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("replay failed: {e}");
            return ExitCode::from(1);
        }
    };

    match serde_json::to_string_pretty(&shard) {
        Ok(json) => println!("{json}"),
        Err(e) => {
            eprintln!("json error: {e}");
            return ExitCode::from(1);
        }
    }

    if let Err(e) = persist_score_shard(db_path, &shard) {
        eprintln!("persist failed: {e}");
        return ExitCode::from(1);
    }

    if !shard.corridor_ok {
        eprintln!(
            "CORRIDOR FAIL: {} invariant violations detected; merge blocked.",
            shard.steps_violated
        );
        return ExitCode::from(2);
    }

    ExitCode::SUCCESS
}
