//! SAFE_FLAG wrapper for iCE40 governance FPGA.
//!
//! This module provides a minimal Rust-side representation of the
//! SAFE_FLAG signal, suitable for Kani proofs. Real hardware access
//! is abstracted behind trait-based interfaces to keep this non-actuating.
//! [file:23][file:41]

#![forbid(unsafe_code)]

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SafeFlagState {
    /// Hardware indicates safe corridor (actuation permitted).
    High,
    /// Hardware indicates unsafe state (STOP asserted).
    Low,
}

/// Pure, in-memory model of SAFE_FLAG evolution.
/// This is what Kani reasons over; hardware mappings are tested
/// separately with integration tests on the iCE40 board.
/// [file:23][file:41]
#[derive(Clone, Debug)]
pub struct SafeFlagModel {
    state: SafeFlagState,
}

impl SafeFlagModel {
    /// Create a new SAFE_FLAG model, starting from High (safe).
    pub fn new() -> Self {
        Self { state: SafeFlagState::High }
    }

    /// Apply a governance verdict from the ecosafety kernel.
    ///
    /// - If verdict is `true` (safe), SAFE_FLAG stays in its current state.
    /// - If verdict is `false` (unsafe), SAFE_FLAG becomes Low and
    ///   stays Low thereafter.
    ///
    /// This enforces monotone behavior: High -> Low is allowed; Low -> High
    /// is disallowed without an external reset not modeled here. [file:23]
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

    pub fn state(&self) -> SafeFlagState {
        self.state
    }
}
