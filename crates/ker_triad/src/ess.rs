// eco_restoration_shard/crates/ker_triad/src/ess.rs
//! ROLE: ESS distribution helpers and sampling stubs.
//! - Minimal functions to produce ESS samples for KS testing.
//! - In a full implementation, these replay hex‑stamped scenario sets.

#![forbid(unsafe_code)]

use rand::Rng;
use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};

use crate::EssSnapshot;

/// Simple ESS sample container.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EssSample {
    pub ess_values: Vec<Decimal>,
}

/// Stub: sample ESS values for a "previous" version.
///
/// Real code will:
/// - Load a fixed scenario set from qpudatashard / ALN.
/// - Replay it using the previous ker_triad implementation.
/// Here we just generate pseudo‑random decimals in \([0.6, 0.9]\) as a stand‑in.
pub fn ess_sample_previous_stub(count: usize) -> EssSample {
    let mut rng = rand::thread_rng();
    let mut values = Vec::with_capacity(count);
    for _ in 0..count {
        let x: f64 = rng.gen_range(0.6..0.9);
        values.push(Decimal::from_f64_retain(x).unwrap_or(Decimal::from_f32(0.75).unwrap()));
    }
    EssSample { ess_values: values }
}

/// Stub: sample ESS values for the "current" version.
///
/// Real code will call the current ker_triad engine on the same scenario set;
/// here we shift the range slightly upward to mimic dominance.
pub fn ess_sample_current_stub(count: usize) -> EssSample {
    let mut rng = rand::thread_rng();
    let mut values = Vec::with_capacity(count);
    for _ in 0..count {
        let x: f64 = rng.gen_range(0.65..0.92);
        values.push(Decimal::from_f64_retain(x).unwrap_or(Decimal::from_f32(0.80).unwrap()));
    }
    EssSample { ess_values: values }
}

/// Convert ESS decimals to f64 for KS machinery.
/// This keeps numeric kernels pure and deterministic.
pub fn ess_to_f64(samples: &EssSample) -> Vec<f64> {
    samples
        .ess_values
        .iter()
        .map(|d| d.to_f64().unwrap_or(0.0))
        .collect()
}
