// Path: aletheion/erm/funding/tests/qf_plutocracy_resistance.rs

use rust_decimal::Decimal;
use rust_decimal_macros::dec;
use crate::health_qf_core::{compute_qf_weight, QfContext, ResponsibilityScalar, EcoCreditAttenuationParams};
use crate::health_qf_core::quadratic_match_total;

fn make_ctx_health() -> QfContext {
    QfContext {
        responsibility: ResponsibilityScalar(dec!(1.0)),
        attenuation: EcoCreditAttenuationParams {
            alpha: dec!(0.05),
            max_credits_for_qf: dec!(1000),
        },
    }
}

#[test]
fn whale_vs_many_small_contributors() {
    let ctx = make_ctx_health();
    let total = dec!(10000); // e.g., 10k native units

    // Whale scenario: 1 contributor
    let whale_amounts = vec![total];
    let whale_weights: Vec<_> = whale_amounts
        .iter()
        .map(|a| compute_qf_weight(*a, dec!(0), ctx))
        .collect();
    let whale_qf = quadratic_match_total(&whale_weights);

    // Many scenario: 10k contributors
    let n_small = 10_000u32;
    let each = total / Decimal::from(n_small);
    let small_amounts = vec![each; n_small as usize];
    let small_weights: Vec<_> = small_amounts
        .iter()
        .map(|a| compute_qf_weight(*a, dec!(0), ctx))
        .collect();
    let small_qf = quadratic_match_total(&small_weights);

    // Plutocracy resistance expectations: small_qf >> whale_qf
    assert!(small_qf > whale_qf * dec!(5));  // e.g., at least 5x boost
}
