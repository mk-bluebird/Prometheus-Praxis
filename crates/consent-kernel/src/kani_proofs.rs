// eco_restoration_shard/cybercore/crates/consent-kernel/src/kani_proofs.rs

#![cfg(kani)]

use super::*;

#[kani::proof]
fn revoked_is_absorbing_without_valid_envelope() {
    // Arbitrary but small bound for bounded model checking.
    const STEPS: usize = 4;

    // Non-deterministic initial state.
    let mut state = kani::any::<ConsentState>();

    // Track whether we've already reached Revoked at least once.
    let mut seen_revoked = false;

    // Simulate a sequence of envelope-free transitions (env = None, env_ok = false).
    for _ in 0..STEPS {
        if state == ConsentState::Revoked {
            seen_revoked = true;
        }

        let next = next_consent_state(state, None, false)
            .expect("envelope-free transitions should not error");

        state = next;

        if seen_revoked {
            // Once Revoked is observed, we assert it remains Revoked.
            assert!(state == ConsentState::Revoked);
        }
    }
}
