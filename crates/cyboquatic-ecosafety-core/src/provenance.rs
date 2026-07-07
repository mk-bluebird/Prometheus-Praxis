//! Provenance tracking for ecosafety diagnostic pipelines.
//!
//! Each frame in the pipeline appends a `ProvenanceStep` describing
//! what it did. The final output carries a vector of steps, giving an
//! audit trail for KER and governance analysis.

use crate::provenance_detail::ProvenanceDetail;

#[derive(Clone, Debug)]
pub struct ProvenanceStep {
    frame_name: &'static str,
    detail_json: String,
}

impl ProvenanceStep {
    pub fn new(frame_name: &'static str, detail: &ProvenanceDetail) -> Self {
        let detail_json =
            serde_json::to_string(detail).unwrap_or_else(|_| "{\"kind\":\"error\"}".to_string());
        Self { frame_name, detail_json }
    }

    pub fn frame_name(&self) -> &'static str {
        self.frame_name
    }

    pub fn detail_json(&self) -> &str {
        &self.detail_json
    }

    pub fn detail(&self) -> Option<ProvenanceDetail> {
        serde_json::from_str(&self.detail_json).ok()
    }
}

#[derive(Clone, Debug, Default)]
pub struct Provenance {
    steps: Vec<ProvenanceStep>,
}

impl Provenance {
    pub fn new() -> Self {
        Self { steps: Vec::new() }
    }

    pub fn push(&mut self, step: ProvenanceStep) {
        self.steps.push(step);
    }

    pub fn steps(&self) -> &[ProvenanceStep] {
        &self.steps
    }
}
