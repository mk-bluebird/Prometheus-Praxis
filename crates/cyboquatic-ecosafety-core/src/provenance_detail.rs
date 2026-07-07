//! Structured provenance detail payloads for ecosafety frames.
//!
//! These payloads are JSON-encoded inside ProvenanceStep so that both
//! humans and AI tools can parse them deterministically.

use serde::{Deserialize, Serialize};

/// Per-frame structured provenance payload.
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum ProvenanceDetail {
    /// Integrity check result for an input window.
    Integrity {
        min_samples: usize,
        samples_present: usize,
        accepted: bool,
    },
    /// Covariance and ecosafety classification result.
    Covariance {
        ecosafety_status: String,
        ecosafety_distance: f32,
        samples_used: u32,
        cov_condition_number: f32,
    },
    /// Biodiversity refinement result.
    Biodiversity {
        r_biodiv_mean: f32,
        r_biodiv_threshold: f32,
        warn: bool,
        dr_biodiv_dr_pfas: f32,
    },
}
