//! Provenance tracking for ecosafety diagnostic pipelines.
//!
//! Each frame in the pipeline appends a `ProvenanceStep` describing
//! what it did. The final output carries a vector of steps, giving an
//! audit trail for KER and governance analysis.

/// Single step in a diagnostic pipeline.
#[derive(Clone, Debug)]
pub struct ProvenanceStep {
    frame_name: &'static str,
    detail: String,
}

impl ProvenanceStep {
    /// Creates a new provenance step.
    pub fn new(frame_name: &'static str, detail: String) -> Self {
        Self { frame_name, detail }
    }

    /// Name of the frame that produced this step.
    pub fn frame_name(&self) -> &'static str {
        self.frame_name
    }

    /// Human‑readable details (e.g., status, flags).
    pub fn detail(&self) -> &str {
        &self.detail
    }
}

/// Provenance attached to ecosafety outputs.
#[derive(Clone, Debug, Default)]
pub struct Provenance {
    steps: Vec<ProvenanceStep>,
}

impl Provenance {
    /// Creates an empty provenance chain.
    pub fn new() -> Self {
        Self { steps: Vec::new() }
    }

    /// Adds a step to the chain.
    pub fn push(&mut self, step: ProvenanceStep) {
        self.steps.push(step);
    }

    /// Returns all steps.
    pub fn steps(&self) -> &[ProvenanceStep] {
        &self.steps
    }
}
