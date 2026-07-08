// filename: eco_restoration_shard/crates/cyboquatic_progress/20260707_crate/src/bin/demo.rs

#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Small demo binary to show how to compute and record today's drainage-decay
//! indicator for a Phoenix segment in a local SQLite file.

use cyboquatic_progress_20260707::compute_and_record_today;

fn main() {
    let db_path = "eco_restoration_shard_progress.db";
    let prior_pointer = "crates/cyboquatic_progress/20260706_crate";

    match compute_and_record_today(
        db_path,
        "phx_segment_001",
        45.0,   // mean TSS mg/L
        -2.0,   // improving trend
        0.4,    // BOD index
        0.7,    // CEC index
        prior_pointer,
    ) {
        Ok((indicator, row_id)) => {
            println!(
                "Recorded drainage-decay indicator for segment {} with score {:.3}, row id={}",
                indicator.segment_id, indicator.drainage_decay_score, row_id
            );
        }
        Err(e) => {
            eprintln!("Failed to record today's progress: {e}");
            std::process::exit(1);
        }
    }
}
