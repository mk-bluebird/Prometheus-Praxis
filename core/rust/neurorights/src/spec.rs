use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum NeurorightCorridorKind {
    BiosignalAcquisition,
    FeatureSpace,
    InferenceSpace,
    Retention,
    Actuation,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Range01 {
    #[serde(rename = "min_01")]
    pub min_01: f64,
    #[serde(rename = "max_01")]
    pub max_01: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct NeurorightCorridorSpec {
    #[serde(rename = "corridor_id")]
    pub corridor_id: String, // koid-style id

    #[serde(rename = "kind")]
    pub kind: NeurorightCorridorKind,

    #[serde(rename = "label")]
    pub label: String,

    #[serde(rename = "allowed_feature_space")]
    pub allowed_feature_space: Vec<String>, // enumerated feature ids

    #[serde(rename = "disallowed_inference_space")]
    pub disallowed_inference_space: Vec<String>, // inference ids

    #[serde(rename = "value_range_01")]
    pub value_range_01: Range01,

    #[serde(rename = "max_retention_days")]
    pub max_retention_days: u32,

    #[serde(rename = "hashonly_required")]
    pub hashonly_required: bool,

    #[serde(rename = "no_downgrade_for_non_disclosure")]
    pub no_downgrade_for_non_disclosure: bool,
}

#[derive(Debug, Error)]
pub enum NeurorightCorridorError {
    #[error("invalid id: empty")]
    EmptyId,
    #[error("invalid label: empty")]
    EmptyLabel,
    #[error("invalid range: min_01({min}) < 0.0 or max_01({max}) > 1.0")]
    RangeOutOfBounds { min: f64, max: f64 },
    #[error("invalid range ordering: min_01({min}) > max_01({max})")]
    RangeOrdering { min: f64, max: f64 },
    #[error("max_retention_days must be > 0 when hashonly_required = false")]
    RetentionTooLow,
}

impl NeurorightCorridorSpec {
    pub fn validate(&self) -> Result<(), NeurorightCorridorError> {
        if self.corridor_id.trim().is_empty() {
            return Err(NeurorightCorridorError::EmptyId);
        }
        if self.label.trim().is_empty() {
            return Err(NeurorightCorridorError::EmptyLabel);
        }

        let r = &self.value_range_01;
        if r.min_01 < 0.0 || r.max_01 > 1.0 {
            return Err(NeurorightCorridorError::RangeOutOfBounds {
                min: r.min_01,
                max: r.max_01,
            });
        }
        if r.min_01 > r.max_01 {
            return Err(NeurorightCorridorError::RangeOrdering {
                min: r.min_01,
                max: r.max_01,
            });
        }

        if !self.hashonly_required && self.max_retention_days == 0 {
            return Err(NeurorightCorridorError::RetentionTooLow);
        }

        Ok(())
    }
}
