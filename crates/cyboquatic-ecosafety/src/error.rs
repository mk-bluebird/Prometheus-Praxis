// filename: crates/cyboquatic-ecosafety/src/error.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use core::fmt;

/// Structured error type for ecosafety frames and engines.
///
/// All covariance, normalization, and risk-coordinate computations must
/// return these errors instead of panicking. This enables governance
/// surfaces, audit shards, and ALN contracts to inspect failures.
#[derive(Debug, Clone)]
pub enum FrameError {
    MissingField(&'static str),
    InvalidRange(&'static str),
    NumericIssue(&'static str),
    InvariantViolation(String),
    Internal(String),
}

impl fmt::Display for FrameError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            FrameError::MissingField(name) => {
                write!(f, "missing required field '{}'", name)
            }
            FrameError::InvalidRange(name) => {
                write!(f, "value out of allowed range for '{}'", name)
            }
            FrameError::NumericIssue(msg) => write!(f, "numeric issue: {}", msg),
            FrameError::InvariantViolation(msg) => write!(f, "invariant violation: {}", msg),
            FrameError::Internal(msg) => write!(f, "internal error: {}", msg),
        }
    }
}

impl core::error::Error for FrameError {}

/// Helper for constructing invariant violation errors from ALN contracts.
pub fn invariant_error<T: Into<String>>(msg: T) -> FrameError {
    FrameError::InvariantViolation(msg.into())
}
