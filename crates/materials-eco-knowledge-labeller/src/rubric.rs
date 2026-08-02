// File: crates/materials-eco-knowledge-labeller/src/rubric.rs
//! Eco‑knowledge rubric and aggregation logic.

use crate::model::MaterialLabel;

/// Aggregate rubric sub‑scores into a single eco‑knowledge factor `k_material`.
///
/// The weights can be tuned; this implementation keeps them simple and explicit.
/// All inputs are assumed to be in [0,1].
pub fn aggregate_k(
    evidential_score: f64,
    quantified_score: f64,
    transparency_score: f64,
    measurability_score: f64,
) -> f64 {
    let raw = 0.3 * evidential_score
        + 0.3 * quantified_score
        + 0.2 * transparency_score
        + 0.2 * measurability_score;

    if raw < 0.0 {
        0.0
    } else if raw > 1.0 {
        1.0
    } else {
        raw
    }
}

/// Construct a `MaterialLabel` from rubric sub‑scores and metadata.
///
/// This does not do interactive input; higher‑level code (CLI/UI) will gather
/// scores from an expert and then call this function to produce a label.
pub fn make_label(
    material_id: String,
    evidential_score: f64,
    quantified_score: f64,
    transparency_score: f64,
    measurability_score: f64,
    annotator: String,
) -> MaterialLabel {
    let k_material = aggregate_k(
        evidential_score,
        quantified_score,
        transparency_score,
        measurability_score,
    );
    MaterialLabel {
        material_id,
        evidential_score,
        quantified_score,
        transparency_score,
        measurability_score,
        k_material,
        annotator,
    }
}
