//! Ecosafety pipeline demo (non-actuating).
//!
//! This example shows how to:
//! - Construct an `EcosafetyPipeline3` (Integrity → Covariance → Biodiversity).
//! - Wrap it with `FrameTimeout` to enforce a hard execution budget.
//! - Feed a synthetic `EcosafetyInputWindow` and print diagnostics.
//!
//! The example is intended for local testing and CI demonstration. It
//! performs no IO beyond stdout and never calls any actuation surfaces.

#![forbid(unsafe_code)]

use std::time::Duration;

// Crate imports.
use cyboquatic_ecosafety::{
    buildecosafetypipeline3, EcosafetyConfig, EcosafetyCovarianceFrame, EcosafetyInputWindow,
    Frame, KerFactors, NodeRiskSample,
};
use cyboquatic_ecosafety::frame_timeout::FrameTimeout;

/// Build a synthetic configuration for the covariance frame.
fn example_config() -> EcosafetyConfig {
    EcosafetyConfig {
        // Thresholds are illustrative and must be tuned against real Phoenix data.
        dwarn_default: 2.0,
        dmax_default: 4.0,
        min_samples: 5,
        max_cov_condition: 1.0e5,
        k_factor_default: 0.94,
        e_factor_default: 0.90,
        r_factor_default: 0.13,
    }
}

/// Build a sample KER factors helper (if the config uses dynamic KER).
fn example_ker_factors() -> KerFactors {
    KerFactors::new(0.94, 0.90, 0.13)
}

/// Build a synthetic input window with a handful of risk samples.
///
/// In a real deployment, this would be filled from SQLite or streaming
/// telemetry, but the pipeline remains purely diagnostic.
fn example_window() -> EcosafetyInputWindow {
    use chrono::{TimeZone, Utc};

    let nodeid = "PHX-CYBOQUATIC-NODE-01".to_string();
    let region = "Phoenix".to_string();
    let medium = "water".to_string();

    let window_start_utc = Utc.with_ymd_and_hms(2026, 7, 7, 0, 0, 0).unwrap();
    let window_end_utc = Utc.with_ymd_and_hms(2026, 7, 7, 1, 0, 0).unwrap();

    let samples = vec![
        NodeRiskSample::new(0.10, 0.12, 0.05, 0.04, 0.08, 0.07, 0.03),
        NodeRiskSample::new(0.11, 0.13, 0.06, 0.05, 0.09, 0.08, 0.04),
        NodeRiskSample::new(0.09, 0.11, 0.04, 0.03, 0.07, 0.06, 0.02),
        NodeRiskSample::new(0.10, 0.12, 0.05, 0.04, 0.08, 0.07, 0.03),
        NodeRiskSample::new(0.11, 0.13, 0.06, 0.05, 0.09, 0.08, 0.04),
    ];

    EcosafetyInputWindow {
        nodeid,
        region,
        medium,
        window_start_utc,
        window_end_utc,
        samples,
        vtatwindowend: Some(0.0),
        lane: Some("EXP".to_string()),
    }
}

fn main() {
    // 1. Build covariance frame and config.
    let config = example_config();
    let ker_factors = example_ker_factors();

    let covariance_frame = EcosafetyCovarianceFrame::new(config, ker_factors);

    // 2. Build the three-stage pipeline (Integrity → Covariance → Biodiversity).
    let min_samples = 3;
    let max_samples = 100;
    let pipeline = buildecosafetypipeline3(min_samples, max_samples, covariance_frame);

    // 3. Wrap the pipeline in a time-bounded frame.
    //
    //    Here we allow a generous 500 ms budget. In CI or production,
    //    this can be tightened once benchmarks exist.
    let timeout_ms = 500;
    let timed_pipeline = FrameTimeout::new(pipeline, timeout_ms);

    // 4. Prepare a synthetic window and run the pipeline.
    let window = example_window();

    let start = std::time::Instant::now();
    let result = timed_pipeline.evaluate(window);
    let elapsed = start.elapsed();

    println!(
        "Pipeline demo: elapsed = {} ms (budget = {} ms)",
        elapsed_as_ms(elapsed),
        timeout_ms
    );

    match result {
        Some(output) => {
            let env = output.envelope;
            println!("Pipeline completed within timeout.");
            println!("Node: {}", env.nodeid());
            println!(
                "Status: {} (lane = {}, kerdeployable = {})",
                env.ecosafety_status(),
                env.lane(),
                env.kerdeployable()
            );
            println!(
                "Distance d = {:.4}, d^2 = {:.4}, dwarn = {:.4}, dmax = {:.4}",
                env.ecosafety_distance(),
                env.ecosafety_distance_sq(),
                env.dwarn(),
                env.dmax()
            );
            println!(
                "KER: K = {:.3}, E = {:.3}, R = {:.3}",
                env.k_factor(),
                env.e_factor(),
                env.r_factor()
            );

            println!("Provenance steps:");
            for step in output.provenance.steps() {
                println!("  - {}: {}", step.stage_name(), step.summary());
            }
        }
        None => {
            println!("Pipeline timed out or rejected input window.");
        }
    }

    // The example is purely diagnostic: no IO other than stdout, no DB,
    // no network, and no actuation verbs.
}

fn elapsed_as_ms(d: Duration) -> u128 {
    d.as_millis()
}
