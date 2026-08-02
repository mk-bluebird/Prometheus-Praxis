// Path suggestion inside Prometheus-Praxis mono-repo:
// src/urban_climate/evaluation/urban_climate_model_evaluation.rs

#![allow(non_snake_case)]
// Edition 2024, rust-version = "1.85" to match project rules.

use alncore::parse_aln_str; // As referenced in governance framework.[8]
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ComponentKind {
    AdvectionKernel,
    MARLStack,
    StreamingPipeline,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UrbanClimateModelComponent {
    pub id: String,
    pub kind: ComponentKind,
    pub description: String,

    pub ker_score: f32,
    pub roh_score: f32,
    pub biodiversity_score: f32,
    pub ecological_planes_score: f32,
    pub urban_corridors_score: f32,
    pub streaming_sla_score: f32,
    pub neurorights_score: f32,

    pub governance_ref: String,
    pub ecology_ref: String,
    pub notes: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UrbanClimateModelEvaluationEnvelope {
    pub envelope_id: String,
    pub region_context: String,
    pub components: Vec<UrbanClimateModelComponent>,
    pub created_at_utc: String, // or chrono::DateTime<Utc>
}

// Thresholds from UrbanClimateModelEvaluation2026v1.aln
pub const MIN_KER_SCORE: f32             = 4.0;
pub const MIN_ROH_SCORE: f32             = 4.0;
pub const MIN_BIODIVERSITY_SCORE: f32    = 4.0;
pub const MIN_ECO_PLANES_SCORE: f32      = 4.0;
pub const MIN_URBAN_CORRIDORS_SCORE: f32 = 3.0;
pub const MIN_STREAMING_SLA_SCORE: f32   = 3.0;
pub const MIN_NEURORIGHTS_SCORE: f32     = 3.0;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum EligibilityVerdict {
    Eligible,
    Ineligible,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EligibilityDecision {
    pub component_id: String,
    pub verdict: EligibilityVerdict,
    pub reason: String,
}

/// Guard implementing MinScoresInvariant from ALN shard.[8][48]
pub fn min_scores_invariant(c: &UrbanClimateModelComponent) -> bool {
    c.ker_score             >= MIN_KER_SCORE &&
    c.roh_score             >= MIN_ROH_SCORE &&
    c.biodiversity_score    >= MIN_BIODIVERSITY_SCORE &&
    c.ecological_planes_score >= MIN_ECO_PLANES_SCORE &&
    c.urban_corridors_score >= MIN_URBAN_CORRIDORS_SCORE &&
    c.streaming_sla_score   >= MIN_STREAMING_SLA_SCORE &&
    c.neurorights_score     >= MIN_NEURORIGHTS_SCORE
}

/// Equivalent of EnforceUrbanClimateModelEligibility policy.[8]
pub fn enforce_urban_climate_model_eligibility(
    env: &UrbanClimateModelEvaluationEnvelope,
) -> Vec<EligibilityDecision> {
    env.components
        .iter()
        .map(|c| {
            if min_scores_invariant(c) {
                EligibilityDecision {
                    component_id: c.id.clone(),
                    verdict: EligibilityVerdict::Eligible,
                    reason: "Scores meet minimum deployment thresholds.".to_string(),
                }
            } else {
                EligibilityDecision {
                    component_id: c.id.clone(),
                    verdict: EligibilityVerdict::Ineligible,
                    reason: "Scores below minimum thresholds; see MinScoresInvariant.".to_string(),
                }
            }
        })
        .collect()
}
