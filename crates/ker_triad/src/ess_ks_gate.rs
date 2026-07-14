//! ROLE: Kolmogorov–Smirnov first‑order stochastic dominance gate for ESS.
//! - Computes KS statistic on previous vs current ESS distributions.
//! - Reports a boolean dominance verdict used by CI / release gating.

#![forbid(unsafe_code)]

use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};
use statrs::statistics::Distribution;
use statrs::distribution::KolmogorovSmirnov;

use crate::ess::{ess_sample_previous_stub, ess_sample_current_stub, ess_to_f64};

/// KS verdict structure for ESS dominance.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EssKsVerdict {
    pub dominates: bool,
    pub ks_statistic: f64,
    pub p_value: f64,
    pub note: String,
}

/// Compute empirical CDF at x for a sorted sample.
fn empirical_cdf(sorted: &[f64], x: f64) -> f64 {
    if sorted.is_empty() {
        return 0.0;
    }
    let mut count = 0usize;
    for v in sorted {
        if *v <= x {
            count += 1;
        } else {
            break;
        }
    }
    (count as f64) / (sorted.len() as f64)
}

/// First‑order stochastic dominance check with KS statistics.
///
/// For sovereignty, we require F_curr(x) <= F_prev(x) for all x
/// (ESS of current version is stochastically higher, i.e. less probability
/// of falling below any threshold), plus a standard KS statistic and p‑value.
pub fn ess_ks_dominance_stub(sample_size: usize) -> EssKsVerdict {
    let prev = ess_to_f64(&ess_sample_previous_stub(sample_size));
    let curr = ess_to_f64(&ess_sample_current_stub(sample_size));

    let mut prev_sorted = prev.clone();
    let mut curr_sorted = curr.clone();
    prev_sorted.sort_by(|a, b| a.partial_cmp(b).unwrap());
    curr_sorted.sort_by(|a, b| a.partial_cmp(b).unwrap());

    // Union grid of evaluation points.
    let mut grid = prev_sorted.clone();
    grid.extend(curr_sorted.iter().copied());
    grid.sort_by(|a, b| a.partial_cmp(b).unwrap());

    let mut max_diff = 0.0_f64;
    let mut dominates = true;

    for x in &grid {
        let f_prev = empirical_cdf(&prev_sorted, *x);
        let f_curr = empirical_cdf(&curr_sorted, *x);
        let diff = (f_prev - f_curr).abs();
        if diff > max_diff {
            max_diff = diff;
        }
        // Sovereignty condition: current CDF must not exceed previous.
        if f_curr > f_prev {
            dominates = false;
        }
    }

    // KS distribution for approximation (two‑sample).
    let n = prev_sorted.len() as f64;
    let m = curr_sorted.len() as f64;
    let neff = (n * m) / (n + m);
    let ks_dist = KolmogorovSmirnov::new(neff).unwrap_or(KolmogorovSmirnov::new(1.0).unwrap());
    let p_value = 1.0 - ks_dist.cdf(max_diff);

    let note = if dominates {
        format!(
            "ESS current distribution first‑order dominates previous (max |F_prev - F_curr| = {:.4}).",
            max_diff
        )
    } else {
        format!(
            "ESS current distribution does NOT dominate previous (max |F_prev - F_curr| = {:.4}).",
            max_diff
        )
    };

    EssKsVerdict {
        dominates,
        ks_statistic: max_diff,
        p_value,
        note,
    }
}

/// CI‑oriented helper: fail fast if dominance does not hold.
///
/// In real CI, this would return a non‑zero exit code; here we keep a
/// pure function that callers can use to gate publish actions.
pub fn ess_ks_gate_passes(sample_size: usize, alpha: f64) -> bool {
    let verdict = ess_ks_dominance_stub(sample_size);
    verdict.dominates && verdict.p_value >= alpha
}

/// Minimal Kani harness stub name referenced in Cargo metadata.
///
/// The actual Kani proof code will live in a separate `tests` module;
/// this stub keeps the symbol visible for wiring.
pub fn ess_ks_dominance_harness_stub() {
    let verdict = ess_ks_dominance_stub(128);
    // Simple sanity assertion for harness skeleton: KS statistic in [0,1].
    assert!(verdict.ks_statistic >= 0.0 && verdict.ks_statistic <= 1.0);
}
