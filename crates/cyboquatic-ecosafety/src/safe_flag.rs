//! SAFE_FLAG governance signal model for iCE40 Lyapunov kernel.
//!
//! This module provides a minimal, purely logical representation of the
//! SAFE_FLAG signal used to gate actuation in hardware. It is non-actuating
//! and suitable for Kani proofs and production ecosafety kernels.

#![forbid(unsafe_code)]

/// Logical state of SAFE_FLAG as seen by the Rust ecosafety kernel.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SafeFlagState {
    /// Ecosafety kernel considers the system safe (actuation permissible).
    High,
    /// Ecosafety kernel considers the system unsafe (STOP asserted).
    Low,
}

/// Pure state machine model for SAFE_FLAG evolution.
///
/// Invariants:
/// - High -> High and High -> Low transitions are allowed.
/// - Low -> Low transitions are allowed.
/// - Low -> High transitions are disallowed without an external reset
///   (reset is not modeled here).
#[derive(Clone, Debug)]
pub struct SafeFlagModel {
    state: SafeFlagState,
}

impl SafeFlagModel {
    /// Initialize SAFE_FLAG in the safe (High) state.
    pub fn new() -> Self {
        Self {
            state: SafeFlagState::High,
        }
    }

    /// Initialize SAFE_FLAG from an explicit state.
    ///
    /// This allows integration tests and harnesses to start directly
    /// from Low when modeling post-fault behavior.
    pub fn from_state(state: SafeFlagState) -> Self {
        Self { state }
    }

    /// Apply a new ecosafety verdict.
    ///
    /// `verdict_safe` is the result of the internal ecosafety kernel
    /// (Lyapunov + KER + corridor checks). When false, SAFE_FLAG drops
    /// to Low and remains there.
    pub fn apply_verdict(&mut self, verdict_safe: bool) {
        match (self.state, verdict_safe) {
            (SafeFlagState::High, false) => {
                self.state = SafeFlagState::Low;
            }
            (SafeFlagState::High, true) => {
                // remain High
            }
            (SafeFlagState::Low, _) => {
                // once Low, remain Low
            }
        }
    }

    /// Current SAFE_FLAG state.
    pub fn state(&self) -> SafeFlagState {
        self.state
    }

    /// Whether SAFE_FLAG currently permits actuation.
    ///
    /// This is a convenience wrapper for consumers that only need a
    /// boolean gate.
    pub fn is_high(&self) -> bool {
        matches!(self.state, SafeFlagState::High)
    }

    /// Whether SAFE_FLAG currently asserts STOP.
    pub fn is_low(&self) -> bool {
        matches!(self.state, SafeFlagState::Low)
    }
}
