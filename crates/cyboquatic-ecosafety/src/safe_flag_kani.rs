//! Kani proof harness for SAFE_FLAG monotone behavior.
//!
//! This harness proves that under the SafeFlagModel, once SAFE_FLAG
//! drops to Low, it never returns to High without an external reset.

#![cfg(kani)]
#![forbid(unsafe_code)]

use kani::any;
use crate::safe_flag::{SafeFlagModel, SafeFlagState};

#[kani::proof]
fn safe_flag_monotone_low_absorbing() {
    let mut model = SafeFlagModel::new();

    // Model a small sequence of ecosafety verdicts.
    let v1: bool = any();
    let v2: bool = any();
    let v3: bool = any();

    model.apply_verdict(v1);
    let s1 = model.state();

    model.apply_verdict(v2);
    let s2 = model.state();

    model.apply_verdict(v3);
    let s3 = model.state();

    // Absorbing Low: once Low, always Low.
    if s1 == SafeFlagState::Low {
        assert!(s2 == SafeFlagState::Low);
        assert!(s3 == SafeFlagState::Low);
    }

    if s2 == SafeFlagState::Low {
        assert!(s3 == SafeFlagState::Low);
    }

    // Coverage: it is possible to stay High if all verdicts are safe.
    kani::cover!(
        s1 == SafeFlagState::High &&
        s2 == SafeFlagState::High &&
        s3 == SafeFlagState::High,
        "SAFE_FLAG remains High under always-safe verdicts"
    );
}
