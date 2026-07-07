//! Biodiversity refinement frame.
//!
//! This frame consumes an ecosafety envelope and provenance, inspects
//! the biodiversity metrics, and adds tags to provenance when
//! biodiversity risk is high or trending poorly.
//!
//! It remains non‑actuating: it does not modify governance lanes or
//! actuation decisions.

use crate::frame::Frame;
use crate::provenance::{Provenance, ProvenanceStep};
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

/// Non‑actuating biodiversity diagnostic frame.
#[derive(Clone, Debug)]
pub struct BiodiversityFrame {
    /// Threshold above which biodiversity risk is considered high.
    r_biodiv_warn: f32,
}

impl BiodiversityFrame {
    /// Creates a new biodiversity frame with a given warning threshold.
    pub fn new(r_biodiv_warn: f32) -> Self {
        Self { r_biodiv_warn }
    }
}

impl Frame<BiodiversityInput, BiodiversityOutput> for BiodiversityFrame {
    fn evaluate(&self, input: &BiodiversityInput) -> BiodiversityOutput {
        let mut provenance = input.provenance().clone();
        let env = input.envelope();

        let r_biodiv = env.r_biodiv_mean();
        let status = env.ecosafety_status();

        let mut detail = format!("r_biodiv_mean={:.4}, status={}", r_biodiv, status);
        if r_biodiv > self.r_biodiv_warn {
            detail.push_str("; biodiversity_warn=true");
        } else {
            detail.push_str("; biodiversity_warn=false");
        }

        provenance.push(ProvenanceStep::new("BiodiversityFrame", detail));

        BiodiversityInput::new(env.clone(), provenance)
    }
}
