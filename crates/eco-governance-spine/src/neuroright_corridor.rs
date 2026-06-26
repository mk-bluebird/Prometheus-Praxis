// crates/eco-governance-spine/src/neuroright_corridor.rs

#![allow(clippy::derive_partial_eq_without_eq)]

use serde::{Deserialize, Serialize};
use serde_with::{serde_as, DisplayFromStr};
use std::fmt;
use thiserror::Error;

#[derive(Debug, Copy, Clone, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum EvidenceMode {
    TopicVectorOnly,
    RedactedTextSnippet,
    FullTextLocalOnly,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum NeurorightsSafetyBand {
    NeurorightsSafe,
    RequiresReview,
    NeurorightsRejected,
}

#[serde_as]
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct NeurorightCorridorSpec {
    #[serde(rename = "corridor_id")]
    pub corridor_id: String,

    #[serde(rename = "description")]
    pub description: String,

    #[serde(rename = "region_code")]
    pub region_code: String,

    #[serde(rename = "min_stake_eco")]
    pub min_stake_eco: f64,

    #[serde(rename = "min_stake_fiat")]
    pub min_stake_fiat: f64,

    #[serde(rename = "evidence_mode")]
    pub evidence_mode: EvidenceMode,

    #[serde(rename = "safety_band")]
    pub safety_band: NeurorightsSafetyBand,

    #[serde(rename = "max_session_seconds")]
    pub max_session_seconds: u32,

    #[serde(rename = "max_daily_seconds")]
    pub max_daily_seconds: u32,

    #[serde(rename = "topic_dim_min")]
    pub topic_dim_min: u16,

    #[serde(rename = "topic_dim_max")]
    pub topic_dim_max: u16,

    #[serde(rename = "allow_bci_streams")]
    pub allow_bci_streams: bool,

    #[serde(rename = "created_utc_ms")]
    pub created_utc_ms: i64,

    #[serde(rename = "updated_utc_ms")]
    pub updated_utc_ms: i64,

    #[serde_as(as = "Option<DisplayFromStr>")]
    #[serde(rename = "spechashhex")]
    pub spechashhex: Option<String>,
}

#[derive(Debug, Error)]
pub enum NeurorightCorridorError {
    #[error("corridor_id must be non-empty and at most 128 chars")]
    InvalidCorridorId,
    #[error("region_code must be non-empty and at most 32 chars")]
    InvalidRegionCode,
    #[error("description must be non-empty and at most 512 chars")]
    InvalidDescription,
    #[error("min_stake_eco must be >= 0.0")]
    NegativeEcoStake,
    #[error("min_stake_fiat must be >= 0.0")]
    NegativeFiatStake,
    #[error("max_session_seconds must be > 0 and <= 86400")]
    InvalidSessionSeconds,
    #[error("max_daily_seconds must be > 0 and <= 86400")]
    InvalidDailySeconds,
    #[error("topic_dim_min must be >= 8 and <= topic_dim_max")]
    InvalidTopicDimMin,
    #[error("topic_dim_max must be <= 4096 and >= topic_dim_min")]
    InvalidTopicDimMax,
    #[error("updated_utc_ms must be >= created_utc_ms")]
    InvalidTimeOrder,
}

impl NeurorightCorridorSpec {
    pub fn validate(&self) -> Result<(), NeurorightCorridorError> {
        if self.corridor_id.is_empty() || self.corridor_id.len() > 128 {
            return Err(NeurorightCorridorError::InvalidCorridorId);
        }
        if self.region_code.is_empty() || self.region_code.len() > 32 {
            return Err(NeurorightCorridorError::InvalidRegionCode);
        }
        if self.description.is_empty() || self.description.len() > 512 {
            return Err(NeurorightCorridorError::InvalidDescription);
        }
        if self.min_stake_eco < 0.0 {
            return Err(NeurorightCorridorError::NegativeEcoStake);
        }
        if self.min_stake_fiat < 0.0 {
            return Err(NeurorightCorridorError::NegativeFiatStake);
        }
        if self.max_session_seconds == 0 || self.max_session_seconds > 86_400 {
            return Err(NeurorightCorridorError::InvalidSessionSeconds);
        }
        if self.max_daily_seconds == 0 || self.max_daily_seconds > 86_400 {
            return Err(NeurorightCorridorError::InvalidDailySeconds);
        }
        if self.topic_dim_min < 8 || self.topic_dim_min > self.topic_dim_max {
            return Err(NeurorightCorridorError::InvalidTopicDimMin);
        }
        if self.topic_dim_max > 4096 || self.topic_dim_max < self.topic_dim_min {
            return Err(NeurorightCorridorError::InvalidTopicDimMax);
        }
        if self.updated_utc_ms < self.created_utc_ms {
            return Err(NeurorightCorridorError::InvalidTimeOrder);
        }
        Ok(())
    }
}

impl fmt::Display for NeurorightCorridorSpec {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "NeurorightCorridorSpec{{id={}, region={}, mode={:?}, band={:?}}}",
            self.corridor_id, self.region_code, self.evidence_mode, self.safety_band
        )
    }
}
