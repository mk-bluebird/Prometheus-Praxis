// Filename: crates/cyboquatic-ecosafety/src/frame.rs
//! Diagnostic frames and composition traits for ecosafety pipelines.

use serde::{Deserialize, Serialize};

use crate::{LyapunovResidual, LyapunovWeights, RiskVector};

/// Lightweight context shared across diagnostic frames.
///
/// This contains the incoming risk vector and residual and can be
/// extended to carry additional immutable context (e.g. basin ID,
/// SAT cell ID, corridor set identifiers) as ALN evolves.[file:21][file:24]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FrameContext {
    /// Incoming risk vector prior to this frame.
    pub risk_in: RiskVector,
    /// Incoming Lyapunov residual prior to this frame.
    pub residual_in: LyapunovResidual,
    /// Lyapunov weights used across the pipeline.
    pub weights: LyapunovWeights,
}

/// Error type for diagnostic frames.
///
/// Frames are strictly non-actuating and may only fail with
/// diagnostic errors, never side-effect hardware state.[file:21][file:24]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum FrameError {
    /// Input values violate preconditions (e.g. NaN, out of corridor).
    InvalidInput(String),
    /// Corridor or schema misalignment detected.
    CorridorViolation(String),
    /// Governance or provenance issue.
    GovernanceViolation(String),
    /// Generic diagnostic error.
    Other(String),
}

/// Pure, non-actuating ecosafety diagnostic frame.
///
/// A `Frame` consumes a `FrameContext` and produces an updated
/// `RiskVector` and residual, without touching actuators.
/// This keeps ecosafety diagnostics separate from control.[file:21][file:24]
pub trait Frame<I, O> {
    /// Execute a single diagnostic pass.
    ///
    /// Implementations must be pure functions with respect to `ctx`
    /// and `input`, and must not perform any I/O or actuation.
    fn run(&self, ctx: &FrameContext, input: I) -> Result<(RiskVector, LyapunovResidual, O), FrameError>;
}

/// Composite frame chaining multiple diagnostics into a single pipeline.
///
/// This trait lets you compose frames for covariance, Vt tracking,
/// biodiversity, integrity, and governance into a single, ordered
/// diagnostic shell without granting them actuation powers.[file:21][file:23]
pub trait CompositeFrame<I, O> {
    /// Run the full pipeline over the given context and input.
    fn run_composite(&self, ctx: &FrameContext, input: I) -> Result<(RiskVector, LyapunovResidual, O), FrameError>;
}

/// Example composite implementation for a pair of frames.
///
/// Library users can construct higher-arity composites by nesting
/// this type or implementing `CompositeFrame` directly.[file:21]
#[derive(Clone)]
pub struct FramePair<F1, F2> {
    f1: F1,
    f2: F2,
}

impl<F1, F2> FramePair<F1, F2> {
    /// Create a new `FramePair` from two frames.
    pub fn new(f1: F1, f2: F2) -> Self {
        Self { f1, f2 }
    }
}

impl<I, M, O, F1, F2> CompositeFrame<I, O> for FramePair<F1, F2>
where
    F1: Frame<I, M>,
    F2: Frame<M, O>,
{
    fn run_composite(
        &self,
        ctx: &FrameContext,
        input: I,
    ) -> Result<(RiskVector, LyapunovResidual, O), FrameError> {
        let (rv_mid, vt_mid, mid) = self.f1.run(ctx, input)?;
        let mid_ctx = FrameContext {
            risk_in: rv_mid,
            residual_in: vt_mid,
            weights: ctx.weights,
        };
        self.f2.run(&mid_ctx, mid)
    }
}
