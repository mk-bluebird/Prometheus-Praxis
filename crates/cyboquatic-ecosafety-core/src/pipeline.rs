//! Ecosafety diagnostic pipeline.
//!
//! This module composes the integrity checks and covariance‑based
//! ecosafety envelope into a single, non‑actuating pipeline.

use crate::config::EcosafetyConfig;
use crate::ecosafety_covariance_frame::{EcosafetyCovarianceFrame, EcosafetyInputWindow};
use crate::frame::{CompositeFrame, Frame};
use crate::integrity_frame::{IntegrityCheckFrame, IntegrityOutput};
use crate::types::CyboNodeEcosafetyEnvelope;

/// Final pipeline type: integrity check → covariance frame.
///
/// Input:  `EcosafetyInputWindow`
/// Output: `Option<CyboNodeEcosafetyEnvelope>`
pub type EcosafetyPipelineFrame =
    CompositeFrame<IntegrityStage, CovarianceStage, EcosafetyInputWindow, IntegrityOutput, Option<CyboNodeEcosafetyEnvelope>>;

/// Thin wrapper stage around `IntegrityCheckFrame` so we can keep the
/// intermediate type explicit.
#[derive(Clone, Debug)]
pub struct IntegrityStage {
    inner: IntegrityCheckFrame,
}

impl IntegrityStage {
    /// Creates a new integrity stage.
    pub fn new(cfg: EcosafetyConfig) -> Self {
        Self {
            inner: IntegrityCheckFrame::new(cfg),
        }
    }
}

impl Frame<EcosafetyInputWindow, IntegrityOutput> for IntegrityStage {
    fn evaluate(&self, input: &EcosafetyInputWindow) -> IntegrityOutput {
        self.inner.evaluate(input)
    }
}

/// Thin wrapper stage around `EcosafetyCovarianceFrame`.
#[derive(Clone, Debug)]
pub struct CovarianceStage {
    inner: EcosafetyCovarianceFrame,
}

impl CovarianceStage {
    /// Creates a new covariance stage.
    pub fn new(cfg: EcosafetyConfig) -> Self {
        Self {
            inner: EcosafetyCovarianceFrame::new(cfg),
        }
    }
}

impl Frame<IntegrityOutput, Option<CyboNodeEcosafetyEnvelope>> for CovarianceStage {
    fn evaluate(&self, intermediate: &IntegrityOutput) -> Option<CyboNodeEcosafetyEnvelope> {
        match intermediate {
            Some(window) => self.inner.evaluate(window),
            None => None,
        }
    }
}

/// Helper constructor for the full pipeline.
pub fn build_ecosafety_pipeline(cfg: EcosafetyConfig) -> EcosafetyPipelineFrame {
    let integrity = IntegrityStage::new(cfg.clone());
    let covariance = CovarianceStage::new(cfg);
    CompositeFrame::new(integrity, covariance)
}
