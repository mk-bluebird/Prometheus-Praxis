//! Three-stage ecosafety pipeline:
//! IntegrityCheckFrame → EcosafetyCovarianceFrame → BiodiversityFrame
//!
//! The final output is an envelope plus a provenance chain describing
//! which frames ran and what they observed.

use crate::biodiversity_frame::{BiodiversityFrame, BiodiversityInput, BiodiversityOutput};
use crate::config::EcosafetyConfig;
use crate::ecosafety_covariance_frame::{EcosafetyCovarianceFrame, EcosafetyInputWindow};
use crate::frame::{CompositeFrame, Frame};
use crate::integrity_frame::{IntegrityCheckFrame, IntegrityOutput};
use crate::provenance::{Provenance, ProvenanceStep};
use crate::provenance_detail::ProvenanceDetail;
use crate::types::CyboNodeEcosafetyEnvelope;

/// Final output: envelope plus provenance chain.
#[derive(Clone, Debug)]
pub struct EcosafetyPipelineOutput {
    envelope: CyboNodeEcosafetyEnvelope,
    provenance: Provenance,
}

impl EcosafetyPipelineOutput {
    pub fn envelope(&self) -> &CyboNodeEcosafetyEnvelope {
        &self.envelope
    }

    pub fn provenance(&self) -> &Provenance {
        &self.provenance
    }
}

/// Stage 1: integrity.
#[derive(Clone, Debug)]
pub struct IntegrityStage {
    inner: IntegrityCheckFrame,
    cfg: EcosafetyConfig,
}

impl IntegrityStage {
    pub fn new(cfg: EcosafetyConfig) -> Self {
        Self {
            inner: IntegrityCheckFrame::new(cfg.clone()),
            cfg,
        }
    }
}

impl Frame<EcosafetyInputWindow, (IntegrityOutput, Provenance)> for IntegrityStage {
    fn evaluate(&self, input: &EcosafetyInputWindow) -> (IntegrityOutput, Provenance) {
        let samples = input.samples();
        let out = self.inner.evaluate(input);

        let mut provenance = Provenance::new();
        let detail = ProvenanceDetail::Integrity {
            min_samples: self.cfg.min_samples,
            samples_present: samples.len(),
            accepted: out.is_some(),
        };
        provenance.push(ProvenanceStep::new("IntegrityCheckFrame", &detail));

        (out, provenance)
    }
}

/// Stage 2: covariance.
#[derive(Clone, Debug)]
pub struct CovarianceStage {
    inner: EcosafetyCovarianceFrame,
}

impl CovarianceStage {
    pub fn new(cfg: EcosafetyConfig) -> Self {
        Self {
            inner: EcosafetyCovarianceFrame::new(cfg),
        }
    }
}

impl Frame<(IntegrityOutput, Provenance), Option<BiodiversityInput>> for CovarianceStage {
    fn evaluate(&self, input: &(IntegrityOutput, Provenance)) -> Option<BiodiversityInput> {
        let (integrity_out, mut provenance) = input.clone();

        let window = match integrity_out {
            Some(w) => w,
            None => {
                let detail = ProvenanceDetail::Covariance {
                    ecosafety_status: "SKIPPED".to_string(),
                    ecosafety_distance: 0.0,
                    samples_used: 0,
                    cov_condition_number: f32::INFINITY,
                };
                provenance.push(ProvenanceStep::new("EcosafetyCovarianceFrame", &detail));
                return None;
            }
        };

        let envelope_opt = self.inner.evaluate(&window);
        match envelope_opt {
            Some(env) => {
                let detail = ProvenanceDetail::Covariance {
                    ecosafety_status: env.ecosafety_status().to_string(),
                    ecosafety_distance: env.ecosafety_distance(),
                    samples_used: env.samples_used(),
                    cov_condition_number: env.cov_condition_number(),
                };
                provenance.push(ProvenanceStep::new("EcosafetyCovarianceFrame", &detail));
                Some(BiodiversityInput::new(env, provenance))
            }
            None => {
                let detail = ProvenanceDetail::Covariance {
                    ecosafety_status: "NO_ENVELOPE".to_string(),
                    ecosafety_distance: 0.0,
                    samples_used: 0,
                    cov_condition_number: f32::INFINITY,
                };
                provenance.push(ProvenanceStep::new("EcosafetyCovarianceFrame", &detail));
                None
            }
        }
    }
}

/// Stage 3: biodiversity.
#[derive(Clone, Debug)]
pub struct BiodiversityStage {
    inner: BiodiversityFrame,
}

impl BiodiversityStage {
    pub fn new(r_biodiv_warn: f32) -> Self {
        Self {
            inner: BiodiversityFrame::new(r_biodiv_warn),
        }
    }
}

impl Frame<Option<BiodiversityInput>, Option<EcosafetyPipelineOutput>> for BiodiversityStage {
    fn evaluate(&self, input: &Option<BiodiversityInput>) -> Option<EcosafetyPipelineOutput> {
        let biodiv_in = match input {
            Some(b) => b.clone(),
            None => return None,
        };

        let biodiv_out = self.inner.evaluate(&biodiv_in);

        Some(EcosafetyPipelineOutput {
            envelope: biodiv_out.envelope().clone(),
            provenance: biodiv_out.provenance().clone(),
        })
    }
}

/// Concrete three-stage composite type.
///
/// I = EcosafetyInputWindow
/// M1 = (IntegrityOutput, Provenance)
/// M2 = Option<BiodiversityInput>
/// O = Option<EcosafetyPipelineOutput>
pub type EcosafetyPipeline3 = CompositeFrame<
    IntegrityStage,
    CompositeFrame<
        CovarianceStage,
        BiodiversityStage,
        (IntegrityOutput, Provenance),
        Option<BiodiversityInput>,
        Option<EcosafetyPipelineOutput>,
    >,
    EcosafetyInputWindow,
    (IntegrityOutput, Provenance),
    Option<EcosafetyPipelineOutput>,
>;

/// Helper constructor for the 3-stage pipeline.
pub fn build_ecosafety_pipeline3(cfg: EcosafetyConfig, r_biodiv_warn: f32) -> EcosafetyPipeline3 {
    let stage1 = IntegrityStage::new(cfg.clone());
    let stage2 = CovarianceStage::new(cfg);
    let stage3 = BiodiversityStage::new(r_biodiv_warn);

    let inner = CompositeFrame::new(stage2, stage3);
    CompositeFrame::new(stage1, inner)
}
