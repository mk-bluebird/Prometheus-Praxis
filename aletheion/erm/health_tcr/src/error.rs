// Path: aletheion/erm/health_tcr/src/error.rs

use thiserror::Error;

#[derive(Debug, Error)]
pub enum HealthTcrError {
    #[error("validation failed: {reason}")]
    Validation {
        reason: String,
        dataset_id: Option<String>,
        field: Option<String>,
    },

    #[error("labor credit violation for account {account_id}: {reason}")]
    LaborCreditViolation {
        account_id: String,
        reason: String,
    },

    #[error("neuroright corridor breach on dataset {dataset_id}: {detail}")]
    NeurorightCorridorBreach {
        dataset_id: String,
        corridor_id: String,
        detail: String,
    },

    #[error("FPIC / treaty gate failure for dataset {dataset_id}: {detail}")]
    TreatyGateFailure {
        dataset_id: String,
        treaty_id: String,
        detail: String,
    },

    #[error("BCI challenge invalid for brain DID {brain_did}: {detail}")]
    BciChallengeInvalid {
        brain_did: String,
        detail: String,
    },

    #[error("slash computation error for curator {curator_id}: {detail}")]
    SlashComputationError {
        curator_id: String,
        detail: String,
    },

    #[error("persistence error: {detail}")]
    Persistence {
        detail: String,
    },

    #[error("concurrency conflict on dataset {dataset_id}: {detail}")]
    Concurrency {
        dataset_id: String,
        detail: String,
    },

    #[error("internal invariant violated: {detail}")]
    InvariantViolation {
        detail: String,
    },
}
