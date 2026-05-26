// filename: crates/eco-pricing/src/lib.rs
// destination: eco_restoration_shard/crates/eco-pricing/src/lib.rs

//! Eco-pricing primitives for EcoNet and Eco-Restoration-Shard.
//!
//! This crate is **non-actuating**: it only exposes pure functions and
//! data structures for computing ecological prices and scores from
//! already-collected metrics (energy, carbon, latency, risk, restoration).
//!
//! It is intended to line up with EcoWealth and related ecometric schemas,
//! but does not perform IO or external calls.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Input metrics for a single action or workload.
/// All values are assumed to be non-negative and in consistent units.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq)]
pub struct EcoMetricInput {
    /// Energy consumed in kilojoules.
    pub energy_kj: f64,
    /// Carbon footprint in grams CO2-equivalent.
    pub carbon_gco2: f64,
    /// Latency in milliseconds.
    pub latency_ms: f64,
    /// Risk score in [0, 1], where higher is worse.
    pub risk: f64,
    /// Restoration yield (e.g., mass removed, area restored), normalized.
    /// Higher is better and can reduce effective price.
    pub restoration_yield: f64,
}

/// Weighting configuration for eco-pricing.
/// All weights must be in [0, 1] and sum to 1.0.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq)]
pub struct EcoPricingWeights {
    pub w_energy: f64,
    pub w_carbon: f64,
    pub w_latency: f64,
    pub w_risk: f64,
    pub w_restoration: f64,
}

/// Error type for invalid inputs.
#[derive(Debug, Error, PartialEq)]
pub enum EcoPricingError {
    #[error("weight components must be in [0, 1]")]
    InvalidWeightRange,
    #[error("weight components must sum to 1.0 (±1e-6), got {0}")]
    InvalidWeightSum(f64),
    #[error("risk must be in [0, 1], got {0}")]
    InvalidRisk(f64),
    #[error("metrics must be non-negative")]
    NegativeMetric,
}

impl EcoPricingWeights {
    /// Validate weight ranges and sum.
    pub fn validate(self) -> Result<(), EcoPricingError> {
        let ws = [
            self.w_energy,
            self.w_carbon,
            self.w_latency,
            self.w_risk,
            self.w_restoration,
        ];

        for w in ws {
            if w < 0.0 || w > 1.0 {
                return Err(EcoPricingError::InvalidWeightRange);
            }
        }

        let sum: f64 = ws.iter().sum();
        if (sum - 1.0).abs() > 1e-6 {
            return Err(EcoPricingError::InvalidWeightSum(sum));
        }

        Ok(())
    }
}

/// Normalized eco-wealth score in [0, 1].
/// Higher means better ecological performance (lower cost, higher restoration).
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq)]
pub struct EcoWealthScore {
    pub score: f64,
}

/// Effective price per normalized workload unit.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq)]
pub struct EcoPrice {
    /// Unitless effective price score (higher = more expensive).
    pub price_score: f64,
}

impl EcoMetricInput {
    /// Validate that metrics are within acceptable ranges.
    pub fn validate(&self) -> Result<(), EcoPricingError> {
        if self.energy_kj < 0.0
            || self.carbon_gco2 < 0.0
            || self.latency_ms < 0.0
            || self.restoration_yield < 0.0
        {
            return Err(EcoPricingError::NegativeMetric);
        }
        if self.risk < 0.0 || self.risk > 1.0 {
            return Err(EcoPricingError::InvalidRisk(self.risk));
        }
        Ok(())
    }
}

/// Compute a normalized eco-wealth score in [0, 1] from metrics and weights.
///
/// Intuition:
/// - Lower energy, carbon, latency, and risk improve the score.
/// - Higher restoration yield improves the score.
/// - Weights define the relative importance of each component.
///
/// The normalization assumes metrics have already been scaled into [0, 1]
/// or that upstream callers provide bounded values.
pub fn compute_eco_wealth(
    metrics: EcoMetricInput,
    weights: EcoPricingWeights,
) -> Result<EcoWealthScore, EcoPricingError> {
    metrics.validate()?;
    weights.validate()?;

    // Invert "bad" metrics into [0, 1] goodness components.
    // Caller is responsible for scaling to [0, 1]; we simply treat
    // the inputs as "badness" and invert them.
    let energy_good = 1.0 - metrics.energy_kj.clamp(0.0, 1.0);
    let carbon_good = 1.0 - metrics.carbon_gco2.clamp(0.0, 1.0);
    let latency_good = 1.0 - metrics.latency_ms.clamp(0.0, 1.0);
    let risk_good = 1.0 - metrics.risk.clamp(0.0, 1.0);
    let restoration_good = metrics.restoration_yield.clamp(0.0, 1.0);

    let score = weights.w_energy * energy_good
        + weights.w_carbon * carbon_good
        + weights.w_latency * latency_good
        + weights.w_risk * risk_good
        + weights.w_restoration * restoration_good;

    Ok(EcoWealthScore { score })
}

/// Derive an effective eco price from a wealth score.
/// Simple monotone mapping: higher wealth => lower price.
pub fn price_from_wealth(score: EcoWealthScore) -> EcoPrice {
    // Price is in [0, 1], with 1 - score mapping.
    let price_score = (1.0 - score.score).clamp(0.0, 1.0);
    EcoPrice { price_score }
}

/// Convenience: compute price directly from metrics and weights.
pub fn compute_eco_price(
    metrics: EcoMetricInput,
    weights: EcoPricingWeights,
) -> Result<EcoPrice, EcoPricingError> {
    let wealth = compute_eco_wealth(metrics, weights)?;
    Ok(price_from_wealth(wealth))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn weights_must_sum_to_one() {
        let weights = EcoPricingWeights {
            w_energy: 0.2,
            w_carbon: 0.2,
            w_latency: 0.2,
            w_risk: 0.2,
            w_restoration: 0.3,
        };
        assert!(weights.validate().is_err());
    }

    #[test]
    fn compute_price_basic() {
        let metrics = EcoMetricInput {
            energy_kj: 0.5,
            carbon_gco2: 0.5,
            latency_ms: 0.5,
            risk: 0.5,
            restoration_yield: 0.5,
        };
        let weights = EcoPricingWeights {
            w_energy: 0.2,
            w_carbon: 0.2,
            w_latency: 0.2,
            w_risk: 0.2,
            w_restoration: 0.2,
        };

        let price = compute_eco_price(metrics, weights).unwrap();
        // Symmetric mid-range metrics should yield mid-range price.
        assert!(price.price_score > 0.0 && price.price_score < 1.0);
    }
}
