// crates/eco-governance-spine/src/health_tcr.rs

use serde::{Deserialize, Serialize};
use std::fmt;
use thiserror::Error;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatasetContext {
    pub dataset_id: String,
    pub region_code: Option<String>,
    pub owner_did: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CorridorContext {
    pub corridor_id: String,
    pub neurorights_safe: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StakeContext {
    pub min_stake_eco: f64,
    pub min_stake_fiat: f64,
    pub provided_stake_eco: f64,
    pub provided_stake_fiat: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EvidenceContext {
    pub evidencehex: String,
    pub signinghex: Option<String>,
}

#[derive(Debug, Error, Serialize, Deserialize)]
pub enum HealthTcrError {
    #[error("dataset not found")]
    DatasetNotFound {
        #[serde(flatten)]
        ctx: DatasetContext,
    },

    #[error("dataset already registered")]
    DatasetAlreadyRegistered {
        #[serde(flatten)]
        ctx: DatasetContext,
    },

    #[error("invalid neurorights corridor binding")]
    InvalidCorridorBinding {
        #[serde(flatten)]
        dataset: DatasetContext,
        #[serde(flatten)]
        corridor: CorridorContext,
    },

    #[error("neurorights corridor not safe for health data")]
    NeurorightsNotSafe {
        #[serde(flatten)]
        dataset: DatasetContext,
        #[serde(flatten)]
        corridor: CorridorContext,
    },

    #[error("stake below required minimums")]
    StakeTooLow {
        #[serde(flatten)]
        dataset: DatasetContext,
        #[serde(flatten)]
        stake: StakeContext,
    },

    #[error("evidence or signature invalid")]
    EvidenceInvalid {
        #[serde(flatten)]
        dataset: DatasetContext,
        #[serde(flatten)]
        evidence: EvidenceContext,
    },

    #[error("evidence mode not allowed for health datasets")]
    EvidenceModeForbidden {
        #[serde(flatten)]
        dataset: DatasetContext,
        evidence_mode: String,
    },

    #[error("transition not allowed from {from_status} to {to_status}")]
    InvalidStatusTransition {
        #[serde(flatten)]
        dataset: DatasetContext,
        from_status: String,
        to_status: String,
    },

    #[error("internal database error: {message}")]
    DatabaseError {
        message: String,
    },

    #[error("unexpected invariant violation: {message}")]
    InvariantViolation {
        message: String,
    },
}

impl HealthTcrError {
    pub fn dataset_id(&self) -> Option<&str> {
        match self {
            HealthTcrError::DatasetNotFound { ctx }
            | HealthTcrError::DatasetAlreadyRegistered { ctx }
            | HealthTcrError::NeurorightsNotSafe { dataset: ctx, .. }
            | HealthTcrError::InvalidCorridorBinding { dataset: ctx, .. }
            | HealthTcrError::StakeTooLow { dataset: ctx, .. }
            | HealthTcrError::EvidenceInvalid { dataset: ctx, .. }
            | HealthTcrError::EvidenceModeForbidden { dataset: ctx, .. }
            | HealthTcrError::InvalidStatusTransition { dataset: ctx, .. } => Some(&ctx.dataset_id),
            HealthTcrError::DatabaseError { .. }
            | HealthTcrError::InvariantViolation { .. } => None,
        }
    }
}

impl fmt::Display for DatasetContext {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "dataset_id={}", self.dataset_id)?;
        if let Some(region) = &self.region_code {
            write!(f, ", region_code={}", region)?;
        }
        if let Some(owner) = &self.owner_did {
            write!(f, ", owner_did={}", owner)?;
        }
        Ok(())
    }
}
