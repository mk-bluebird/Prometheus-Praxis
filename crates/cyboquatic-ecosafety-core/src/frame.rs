//! Core diagnostic frame traits.
//!
//! Frames are pure, non‑actuating computation units that transform
//! input observations (e.g., NodeRiskSample windows) into higher‑level
//! diagnostics (e.g., ecosafety envelopes, tags, shard updates).
//!
//! The `Frame` trait is intentionally generic over input and output,
//! so it can be reused for covariance, Lyapunov, biodiversity, and
//! governance tagging frames.
//!
//! `CompositeFrame` allows chaining multiple frames into a pipeline
//! while preserving strong typing and keeping the entire process
//! non‑actuating.

/// Generic diagnostic frame.
///
/// A `Frame` takes an input value `I` (often a reference) and produces
/// an output value `O`. Implementations must be pure functions of
/// their inputs: they must not perform I/O or actuation.
pub trait Frame<I, O> {
    /// Evaluates the frame on the given input.
    fn evaluate(&self, input: &I) -> O;
}

/// A composite frame that applies a sequence of frames in order.
///
/// This is a simple functional composition helper for building
/// multi‑stage pipelines such as:
///
/// `IntegrityCheckFrame` → `EcosafetyCovarianceFrame` → `BiodiversityFrame`
///
/// without introducing any actuation logic.
pub struct CompositeFrame<F1, F2, I, M, O>
where
    F1: Frame<I, M>,
    F2: Frame<M, O>,
{
    first: F1,
    second: F2,
    _phantom_i: core::marker::PhantomData<I>,
    _phantom_m: core::marker::PhantomData<M>,
    _phantom_o: core::marker::PhantomData<O>,
}

impl<F1, F2, I, M, O> CompositeFrame<F1, F2, I, M, O>
where
    F1: Frame<I, M>,
    F2: Frame<M, O>,
{
    /// Constructs a new composite frame from two stages.
    pub fn new(first: F1, second: F2) -> Self {
        Self {
            first,
            second,
            _phantom_i: core::marker::PhantomData,
            _phantom_m: core::marker::PhantomData,
            _phantom_o: core::marker::PhantomData,
        }
    }
}

impl<F1, F2, I, M, O> Frame<I, O> for CompositeFrame<F1, F2, I, M, O>
where
    F1: Frame<I, M>,
    F2: Frame<M, O>,
{
    fn evaluate(&self, input: &I) -> O {
        let mid = self.first.evaluate(input);
        self.second.evaluate(&mid)
    }
}
