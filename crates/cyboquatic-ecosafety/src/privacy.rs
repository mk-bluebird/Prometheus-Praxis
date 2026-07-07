// filename: crates/cyboquatic-ecosafety/src/privacy.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use core::fmt;

/// Scalar for ecosafety math.
pub type Scalar = f64;

/// Minimal per-node ecosafety statistics that operators are allowed to share
/// (in masked form). This should match fields that are already present in
/// ecosafety RiskVector or NodeRiskSample.
#[derive(Debug, Clone)]
pub struct LocalRiskStats {
    /// Number of node samples in this batch.
    pub sample_count: u64,
    /// Sum of ecosafety risk scalar over samples (e.g., mean of 0..1).
    pub sum_risk: Scalar,
    /// Sum of risk squared (for variance estimates).
    pub sum_risk_sq: Scalar,
}

impl LocalRiskStats {
    pub fn new(sample_count: u64, sum_risk: Scalar, sum_risk_sq: Scalar) -> Self {
        Self {
            sample_count,
            sum_risk,
            sum_risk_sq,
        }
    }
}
