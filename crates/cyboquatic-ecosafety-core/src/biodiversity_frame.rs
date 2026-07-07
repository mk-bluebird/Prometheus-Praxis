//! Biodiversity refinement frame.
//!
//! This frame consumes an ecosafety envelope and provenance, inspects
//! biodiversity metrics, and appends structured provenance about
//! biodiversity risk and sensitivity.
//!
//! It remains non-actuating: it does not modify governance lanes or
//! actuation decisions.

use crate::biodiversity_math::{biodiversity_risk, dr_biodiv_dr_pfas, BiodiversityWeights};
use crate::frame::Frame;
use crate::provenance::{Provenance, ProvenanceStep};
use crate::provenance_detail::ProvenanceDetail;
use crate::types::CyboNodeEcosafetyEnvelope;

/// Input to the biodiversity frame: envelope + provenance.
#[derive(Clone, Debug)]
pub struct BiodiversityInput {
    envelope: CyboNodeEcosafetyEnvelope,
    provenance: Provenance,
}

impl BiodiversityInput {
    /// Creates a new biodiversity input wrapper.
    pub fn new(envelope: CyboNodeEcosafetyEnvelope, provenance: Provenance) -> Self {
        Self { envelope, provenance }
    }

    /// Access to the envelope.
    pub fn envelope(&self) -> &CyboNodeEcosafetyEnvelope {
        &self.envelope
    }

    /// Access to the provenance chain.
    pub fn provenance(&self) -> &Provenance {
        &self.provenance
    }
}

/// Output of biodiversity frame: envelope + updated provenance.
pub type BiodiversityOutput = BiodiversityInput;

/// Non-actuating biodiversity diagnostic frame.
#[derive(Clone, Debug)]
pub struct BiodiversityFrame {
    /// Threshold above which biodiversity risk is considered high.
    r_biodiv_warn: f32,
    /// Sensitivity weights for biodiversity risk function.
    weights: BiodiversityWeights,
}

impl BiodiversityFrame {
    /// Creates a new biodiversity frame with a given warning threshold and weights.
    pub fn new(r_biodiv_warn: f32, weights: BiodiversityWeights) -> Self {
        Self {
            r_biodiv_warn,
            weights,
        }
    }
}

impl Frame<BiodiversityInput, BiodiversityOutput> for BiodiversityFrame {
    fn evaluate(&self, input: &BiodiversityInput) -> BiodiversityOutput {
        let mut provenance = input.provenance().clone();
        let env = input.envelope();

        // Compute biodiversity risk from core coordinates.
        let r_biodiv = biodiversity_risk(
            env.r_pfas_mean(),
            env.r_cec_mean(),
            env.r_trap_fish_mean(),
            env.r_trap_amphib_mean(),
            &self.weights,
        );

        let warn = r_biodiv > self.r_biodiv_warn;

        let dr_dp = dr_biodiv_dr_pfas(
            env.r_cec_mean(),
            env.r_trap_fish_mean(),
            env.r_trap_amphib_mean(),
            &self.weights,
        );

        let detail = ProvenanceDetail::Biodiversity {
            r_biodiv_mean: r_biodiv,
            r_biodiv_threshold: self.r_biodiv_warn,
            warn,
            dr_biodiv_dr_pfas: dr_dp,
        };

        provenance.push(ProvenanceStep::new("BiodiversityFrame", &detail));

        BiodiversityInput::new(env.clone(), provenance)
    }
}
