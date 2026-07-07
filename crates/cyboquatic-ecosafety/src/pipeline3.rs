//! Three-stage ecosafety pipeline with provenance.
//!
//! Stages:
//! 1. IntegrityCheckFrame<I -> IntegrityOutput>
//! 2. EcosafetyCovarianceFrame<IntegrityOutput -> CyboNodeEcosafetyEnvelope>
//! 3. BiodiversityIntegrityFrame (placeholder for rbiodiv-focused logic)
//!
//! The pipeline is non-actuating and focuses only on diagnostics and
//! provenance, suitable for both server-side and edge analytics.

#![forbid(unsafe_code)]

use crate::ecosafetycovarianceframe::{EcosafetyCovarianceFrame, EcosafetyInputWindow};
use crate::frame::Frame;
use crate::integrityframe::{IntegrityCheckFrame, IntegrityOutput};
use crate::types::CyboNodeEcosafetyEnvelope;
use crate::provenance::{Provenance, ProvenanceStep};

/// Output of the full ecosafety pipeline.
#[derive(Clone, Debug)]
pub struct EcosafetyPipelineOutput {
    /// Primary ecosafety envelope for the window.
    pub envelope: CyboNodeEcosafetyEnvelope,
    /// Provenance record capturing frame sequence and hints.
    pub provenance: Provenance,
}

/// Biodiversity-focused integrity frame.
///
/// This is a lightweight hook that can be extended later with richer
/// biodiversity anomaly checks. For now it simply passes through the
/// envelope while tagging provenance.
#[derive(Clone, Debug)]
pub struct BiodiversityIntegrityFrame;

impl BiodiversityIntegrityFrame {
    /// Construct a new biodiversity frame.
    pub fn new() -> Self {
        Self
    }
}

impl Frame<CyboNodeEcosafetyEnvelope, CyboNodeEcosafetyEnvelope> for BiodiversityIntegrityFrame {
    fn evaluate(&self, envelope: CyboNodeEcosafetyEnvelope) -> Option<CyboNodeEcosafetyEnvelope> {
        Some(envelope)
    }
}

/// Three-stage ecosafety pipeline.
#[derive(Clone)]
pub struct EcosafetyPipeline3 {
    integrity: IntegrityCheckFrame,
    covariance: EcosafetyCovarianceFrame,
    biodiversity: BiodiversityIntegrityFrame,
}

impl EcosafetyPipeline3 {
    /// Build a new pipeline from its components.
    pub fn new(
        integrity: IntegrityCheckFrame,
        covariance: EcosafetyCovarianceFrame,
        biodiversity: BiodiversityIntegrityFrame,
    ) -> Self {
        Self {
            integrity,
            covariance,
            biodiversity,
        }
    }

    /// Evaluate the pipeline on a single input window.
    ///
    /// Returns `None` if:
    /// - integrity checks fail, or
    /// - covariance frame rejects the window.
    pub fn evaluate(&self, window: EcosafetyInputWindow) -> Option<EcosafetyPipelineOutput> {
        let mut provenance = Provenance::new();

        // Stage 1: integrity.
        let integrity_out = self.integrity.evaluate(window)?;
        provenance.push(ProvenanceStep::new(
            "IntegrityCheckFrame",
            if integrity_out.accepted() {
                "accepted"
            } else {
                "rejected"
            },
        ));

        if !integrity_out.accepted() {
            return None;
        }

        // Stage 2: covariance ecosafety computation.
        let envelope = self.covariance.evaluate(integrity_out.into_window())?;
        provenance.push(ProvenanceStep::new("EcosafetyCovarianceFrame", "ok"));

        // Stage 3: biodiversity integrity (placeholder).
        let envelope = self.biodiversity.evaluate(envelope)?;
        provenance.push(ProvenanceStep::new("BiodiversityIntegrityFrame", "ok"));

        Some(EcosafetyPipelineOutput { envelope, provenance })
    }
}

/// Convenience constructor for a default pipeline.
///
/// `min_samples` and `max_samples` control integrity thresholds.
/// `covariance` is injected so the caller can configure its
/// numerical settings externally.
pub fn buildecosafetypipeline3(
    min_samples: usize,
    max_samples: usize,
    covariance: EcosafetyCovarianceFrame,
) -> EcosafetyPipeline3 {
    let integrity = IntegrityCheckFrame::new(min_samples, max_samples);
    let biodiversity = BiodiversityIntegrityFrame::new();

    EcosafetyPipeline3::new(integrity, covariance, biodiversity)
}
