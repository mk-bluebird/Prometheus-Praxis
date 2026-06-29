// src/decision_logger.rs

#![forbid(unsafe_code)]

//! Immutable decision logging to Veritas-Chain.

use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use thiserror::Error;

// Local bounded metric type, wired to existing bioscale_metrics semantics.
use bioscale_metrics::Bounded01;

// Veritas chain client abstraction from the ecosystem.
use veritas_chain_client::VeritasChainClient;

/// Errors that can occur during logging.
#[derive(Debug, Error)]
pub enum LoggingError {
    #[error("Serialization failed: {0}")]
    Serialization(String),
    #[error("Chain append failed: {0}")]
    ChainAppend(String),
}

/// A cryptographically traceable decision log entry.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DecisionLog {
    pub decision_id: String,
    pub task_id: String,
    pub allowed: bool,
    pub reasons: String,
    pub hextrace: String,
    pub timestamp_utc: DateTime<Utc>,
    pub k_e_r_vector: String,
}

impl DecisionLog {
    pub fn new(
        decision_id: impl Into<String>,
        task_id: impl Into<String>,
        allowed: bool,
        reasons: impl Into<String>,
        hextrace: impl Into<String>,
        timestamp_utc: DateTime<Utc>,
        k_e_r_vector: impl Into<String>,
    ) -> Self {
        Self {
            decision_id: decision_id.into(),
            task_id: task_id.into(),
            allowed,
            reasons: reasons.into(),
            hextrace: hextrace.into(),
            timestamp_utc,
            k_e_r_vector: k_e_r_vector.into(),
        }
    }
}

/// Logger responsible for formatting and appending decisions.
pub struct DecisionLogger<C> {
    client: C,
}

impl<C> DecisionLogger<C>
where
    C: VeritasChainClient,
{
    /// Creates a new logger wrapping the given chain client.
    pub fn new(client: C) -> Self {
        Self { client }
    }

    /// Logs a decision to the chain.
    pub fn log_decision(&self, log: &DecisionLog) -> Result<(), LoggingError> {
        let payload =
            serde_json::to_vec(log).map_err(|e| LoggingError::Serialization(e.to_string()))?;
        self.client.append(&payload).map_err(|e| LoggingError::ChainAppend(e.to_string()))
    }

    /// Formats the K/E/R vector for the log entry.
    pub fn build_k_e_r_vector(k: Bounded01, e: Bounded01, r: Bounded01) -> String {
        format!(
            "K:{:.3};E:{:.3};R:{:.3}",
            k.into_inner(),
            e.into_inner(),
            r.into_inner()
        )
    }
}

/// Convenience constructor to build and immediately log a decision.
pub fn log_decision_entry<C>(
    logger: &DecisionLogger<C>,
    decision_id: impl Into<String>,
    task_id: impl Into<String>,
    allowed: bool,
    reasons: impl Into<String>,
    hextrace: impl Into<String>,
    k: Bounded01,
    e: Bounded01,
    r: Bounded01,
) -> Result<(), LoggingError>
where
    C: VeritasChainClient,
{
    let k_e_r_vector = DecisionLogger::<C>::build_k_e_r_vector(k, e, r);
    let entry = DecisionLog::new(
        decision_id,
        task_id,
        allowed,
        reasons,
        hextrace,
        Utc::now(),
        k_e_r_vector,
    );
    logger.log_decision(&entry)
}
