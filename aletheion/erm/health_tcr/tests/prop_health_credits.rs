// Path: aletheion/erm/health_tcr/tests/prop_health_credits.rs

use proptest::prelude::*;
use aletheion_erm_health_tcr::{
    apply_labor_events,
    HealthLaborEvent,
    HealthLaborAccountState,
};

proptest! {
    // Generated events always have non-negative deltas and bounded weights.
    #[test]
    fn credits_never_negative(events in prop::collection::vec(arb_labor_event(), 0..500)) {
        let mut state = HealthLaborAccountState::zero();

        for e in events {
            state = apply_labor_events(state, &[e]).expect("apply_labor_events must not panic");
            prop_assert!(state.total_credits >= dec!(0));
        }
    }
}

fn arb_labor_event() -> impl Strategy<Value = HealthLaborEvent> {
    // Example: reward_delta in [-10, 100], but apply_labor_events must clamp at 0.
    ( -10i64..1000i64, 0u8..=100u8 ).prop_map(|(delta_raw, k_raw)| {
        HealthLaborEvent {
            account_id: format!("acct-{k_raw}"),
            reward_delta: Decimal::from_i64(delta_raw).unwrap(),
            info_value_01: Decimal::from_u8(k_raw).unwrap() / dec!(100),
            // ... other bounded fields ...
        }
    })
}
