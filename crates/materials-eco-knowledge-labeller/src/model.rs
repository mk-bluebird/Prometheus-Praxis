// File: crates/materials-eco-knowledge-labeller/src/model.rs
//! Core data structures for material descriptions and labels.

use serde::{Deserialize, Serialize};

/// A raw textual material description ingested from an external source.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialText {
    /// Unique identifier within the corpus.
    pub id: String,
    /// Source tag (e.g., "phoenix_permit", "spec_sheet").
    pub source: String,
    /// Raw textual description of the material.
    pub text: String,
}

/// A structured eco‑knowledge label derived from an expert rubric.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialLabel {
    /// Id of the corresponding material description.
    pub material_id: String,
    /// Evidence density score [0,1]: standards, LCAs, EPDs referenced.
    pub evidential_score: f64,
    /// Quantified eco attributes score [0,1]: explicit percentages, carbon, toxicity.
    pub quantified_score: f64,
    /// Risk transparency score [0,1]: explicit mention of risks vs vague marketing.
    pub transparency_score: f64,
    /// Measurability score [0,1]: how many claims can be verified by tests or databases.
    pub measurability_score: f64,
    /// Aggregated eco‑knowledge factor `k_material` in [0,1].
    pub k_material: f64,
    /// Annotator identifier (e.g., expert handle).
    pub annotator: String,
}
