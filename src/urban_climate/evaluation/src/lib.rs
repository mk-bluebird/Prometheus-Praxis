// Crate entry point for urban_climate_evaluation

pub mod urban_climate_model_evaluation;

#[cfg(kani)]
pub mod urban_climate_model_evaluation_kani;

pub use urban_climate_model_evaluation::{
    ComponentKind,
    UrbanClimateModelComponent,
    UrbanClimateModelEvaluationEnvelope,
    EligibilityVerdict,
    EligibilityDecision,
    enforce_urban_climate_model_eligibility,
};
