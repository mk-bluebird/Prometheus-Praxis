// eco_restoration_shard/crates/prometheus_praxis_collective_feed/src/lib.rs
//
// Prometheus-Praxis Collective Intelligence Feed
// Advisory-only enrichment layer for KER, incorporating community/equity signals
// without ever relaxing RoH, Tsafe, or Lyapunov invariants.
// Edition 2024, rust-version = "1.85", non-actuating, invariant-first.

#![forbid(unsafe_code)]

use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};

/// KER snapshot type reused from the governance kernel for enrichment.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerSnapshot {
    pub k: Decimal,
    pub e: Decimal,
    pub r: Decimal,
}

/// Signal from Noösphere (global knowledge/community mesh).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NoosphereSignal {
    pub topic_id: String,
    pub priority_scalar: Decimal, // 0..1
    pub evidence_strength: Decimal, // 0..1
}

/// Signal from Gemeinschaft-Verein (local/community equity).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GemeinschaftSignal {
    pub community_id: String,
    pub equity_gap_index: Decimal, // 0..1
    pub support_level: Decimal,    // 0..1
}

/// Signal representing innovation uptake (Fortschritt-Link).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InnovationSignal {
    pub innovation_id: String,
    pub uptake_rate: Decimal, // 0..1
    pub eco_benefit_scalar: Decimal, // 0..1
}

/// Aggregated snapshot of collective intelligence signals for a decision.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CollectiveFeedSnapshot {
    pub community_priority_shift: Decimal, // -1..1 (normalized)
    pub equity_gap_index: Decimal,         // 0..1
    pub innovation_uptake_rate: Decimal,   // 0..1
}

/// Advisory configuration limits for KER adjustments.
/// These are strict bounds to prevent collective signals from overpowering safety.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CollectiveFeedConfig {
    pub max_delta_k: Decimal,
    pub max_delta_e: Decimal,
    pub max_delta_r_increase: Decimal,
    pub max_delta_r_decrease: Decimal,
}

impl Default for CollectiveFeedConfig {
    fn default() -> Self {
        Self {
            max_delta_k: Decimal::from_f32(0.05).unwrap(),
            max_delta_e: Decimal::from_f32(0.05).unwrap(),
            max_delta_r_increase: Decimal::from_f32(0.0).unwrap(),
            max_delta_r_decrease: Decimal::from_f32(0.05).unwrap(),
        }
    }
}

/// Aggregate raw signals into a normalized CollectiveFeedSnapshot.
pub fn aggregate_signals(
    noosphere: &[NoosphereSignal],
    gemeinschaft: &[GemeinschaftSignal],
    innovation: &[InnovationSignal],
) -> CollectiveFeedSnapshot {
    let zero = Decimal::from_f32(0.0).unwrap();
    let one = Decimal::from_f32(1.0).unwrap();

    let mut priority_sum = zero;
    let mut priority_weight_sum = zero;
    for s in noosphere {
        priority_sum += s.priority_scalar * s.evidence_strength;
        priority_weight_sum += s.evidence_strength;
    }
    let community_priority_shift = if priority_weight_sum > zero {
        priority_sum / priority_weight_sum
    } else {
        zero
    };

    let mut equity_sum = zero;
    let mut equity_count = zero;
    for g in gemeinschaft {
        equity_sum += g.equity_gap_index;
        equity_count += one;
    }
    let equity_gap_index = if equity_count > zero {
        equity_sum / equity_count
    } else {
        zero
    };

    let mut uptake_sum = zero;
    let mut uptake_count = zero;
    for i in innovation {
        uptake_sum += i.uptake_rate * i.eco_benefit_scalar;
        uptake_count += one;
    }
    let innovation_uptake_rate = if uptake_count > zero {
        uptake_sum / uptake_count
    } else {
        zero
    };

    CollectiveFeedSnapshot {
        community_priority_shift,
        equity_gap_index,
        innovation_uptake_rate,
    }
}

/// Apply advisory K/E/R adjustments within strict bounds.
/// Safety invariants (RoH ceiling, Tsafe, Lyapunov) must be enforced elsewhere
/// and must never be relaxed based on collective signals.
pub fn apply_collective_bias(
    ker: KerSnapshot,
    feed: &CollectiveFeedSnapshot,
    cfg: &CollectiveFeedConfig,
) -> KerSnapshot {
    let zero = Decimal::from_f32(0.0).unwrap();

    // Community priority and innovation uptake can nudge K up.
    let mut delta_k = feed.community_priority_shift * cfg.max_delta_k;
    if delta_k < zero {
        delta_k = zero;
    }
    if delta_k > cfg.max_delta_k {
        delta_k = cfg.max_delta_k;
    }

    // Eco-benefit and innovation uptake can nudge E up.
    let mut delta_e = feed.innovation_uptake_rate * cfg.max_delta_e;
    if delta_e < zero {
        delta_e = zero;
    }
    if delta_e > cfg.max_delta_e {
        delta_e = cfg.max_delta_e;
    }

    // Equity gaps can only reduce R (risk-of-harm) within limits; never increase.
    let mut delta_r = feed.equity_gap_index * cfg.max_delta_r_decrease;
    if delta_r < zero {
        delta_r = zero;
    }
    if delta_r > cfg.max_delta_r_decrease {
        delta_r = cfg.max_delta_r_decrease;
    }

    let k_new = ker.k + delta_k;
    let e_new = ker.e + delta_e;
    let r_new = ker.r - delta_r;

    KerSnapshot {
        k: k_new,
        e: e_new,
        r: r_new,
    }
}

// -----------------------------------------------------------------------------
// Kani harness stubs (module-level, #[cfg(kani)]) documenting advisory properties.
// -----------------------------------------------------------------------------

#[cfg(kani)]
mod kani_harnesses {
    use super::*;
    use rust_decimal::Decimal;

    fn dec(v: f32) -> Decimal {
        Decimal::from_f32(v).unwrap()
    }

    /// Property: apply_collective_bias never increases R beyond the original value.
    #[kani::proof]
    fn kani_collective_bias_never_increases_r() {
        let ker = KerSnapshot {
            k: dec(0.90),
            e: dec(0.88),
            r: dec(0.15),
        };
        let feed = CollectiveFeedSnapshot {
            community_priority_shift: dec(0.5),
            equity_gap_index: dec(0.3),
            innovation_uptake_rate: dec(0.4),
        };
        let cfg = CollectiveFeedConfig::default();

        let ker_new = apply_collective_bias(ker.clone(), &feed, &cfg);
        assert!(ker_new.r <= ker.r);
    }

    /// Property: apply_collective_bias respects max_delta_k and max_delta_e bounds.
    #[kani::proof]
    fn kani_collective_bias_respects_k_e_bounds() {
        let ker = KerSnapshot {
            k: dec(0.80),
            e: dec(0.75),
            r: dec(0.20),
        };
        let feed = CollectiveFeedSnapshot {
            community_priority_shift: dec(1.0),
            equity_gap_index: dec(1.0),
            innovation_uptake_rate: dec(1.0),
        };
        let cfg = CollectiveFeedConfig::default();

        let ker_new = apply_collective_bias(ker.clone(), &feed, &cfg);

        let max_delta_k = cfg.max_delta_k;
        let max_delta_e = cfg.max_delta_e;

        assert!(ker_new.k <= ker.k + max_delta_k);
        assert!(ker_new.e <= ker.e + max_delta_e);
    }
}
