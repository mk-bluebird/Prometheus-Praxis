//! Kani harness for SAFE_FLAG monotone behavior on the Rust side.
//!
//! Proves that, under the SafeFlagModel, SAFE_FLAG never transitions
//! from Low back to High without a reset. [file:23][file:41]

#![cfg(kani)]
#![forbid(unsafe_code)]

use kani::any;
use crate::safe_flag::{SafeFlagModel, SafeFlagState};

#[kani::proof]
fn safe_flag_monotone() {
    // Start from a fresh SAFE_FLAG in High state.
    let mut model = SafeFlagModel::new();

    // Apply a bounded sequence of governance verdicts, each indicating
    // whether the ecosafety kernel considered the system safe.
    //
    // For Kani, we model 3 steps; this is sufficient given the simple
    // state machine and can be extended if needed. [file:23][file:41]
    let v1: bool = any();
    let v2: bool = any();
    let v3: bool = any();

    model.apply_verdict(v1);
    let s1 = model.state();

    model.apply_verdict(v2);
    let s2 = model.state();

    model.apply_verdict(v3);
    let s3 = model.state();

    // Monotonicity: once Low, always Low.
    //
    // If at any step SAFE_FLAG is Low, all subsequent states must be Low.
    if s1 == SafeFlagState::Low {
        assert!(s2 == SafeFlagState::Low);
        assert!(s3 == SafeFlagState::Low);
    }

    if s2 == SafeFlagState::Low {
        assert!(s3 == SafeFlagState::Low);
    }

    // Additional coverage: SAFE_FLAG may remain High if all verdicts are safe.
    kani::cover!(
        s1 == SafeFlagState::High &&
        s2 == SafeFlagState::High &&
        s3 == SafeFlagState::High,
        "SAFE_FLAG remains High under always-safe verdicts"
    );
}
