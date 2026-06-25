// filepath: crates/psychrisk_engine/tests/kani_amber_uplift.rs
#![forbid(unsafe_code)]

use kani::any;

use psychrisk_engine::{
    evaluate_amber_uplift,
    AdultFloorEnvelope,
    MentalIntegrityGuardInputs,
};
use psychrisk_engine::types::{
    MentalIntegrityBinding,
    MentalIntegrityDimensions,
    MentalIntegrityPolicy,
    MentalIntegrityDoctrine,
    PciWindow,
    PciWindowKind,
};

/// Helper to clamp into [0.0, 1.0] for harness construction.
fn clamp01(x: f32) -> f32 {
    if !x.is_finite() {
        0.0
    } else if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

/// Kani verification harness for the Amber uplift safety property:
///
/// uplift_allowed == true ⇒
///   pci_short >= adult_floor.min_pci_short_15m ∧
///   pci_long  >= adult_floor.min_pci_long_24h ∧
///   cogload_delta_per_min <= policy.max_cogload_delta_per_min_amber.
///
/// This relies on the implementation of evaluate_amber_uplift in
/// mental_integrity_guard.rs and the minimal type definitions in types.rs.
#[kani::proof]
fn verify_amber_uplift_respects_pci_and_cogload_slope() {
    // Arbitrary but clamped dimensions.
    let cognitive_integrity = clamp01(any());
    let affective_integrity = clamp01(any());
    let narrative_integrity = clamp01(any());
    let social_integrity = clamp01(any());

    let dims = MentalIntegrityDimensions {
        cognitive_integrity,
        affective_integrity,
        narrative_integrity,
        social_integrity,
    };

    // Adult floor thresholds (also clamped).
    let adult_min_ci = clamp01(any());
    let adult_min_ai = clamp01(any());
    let adult_min_ni = clamp01(any());
    let adult_min_si = clamp01(any());
    let adult_min_pci_short = clamp01(any());
    let adult_min_pci_long = clamp01(any());

    let adult_floor = AdultFloorEnvelope {
        min_cognitive_integrity: adult_min_ci,
        min_affective_integrity: adult_min_ai,
        min_narrative_integrity: adult_min_ni,
        min_social_integrity: adult_min_si,
        min_pci_short_15m: adult_min_pci_short,
        min_pci_long_24h: adult_min_pci_long,
    };

    // Policy thresholds – ensure they are not weaker than the adult floor
    // by construction in the harness (this avoids trivial counterexamples).
    let policy_min_ci = clamp01(any());
    let policy_min_ai = clamp01(any());
    let policy_min_ni = clamp01(any());
    let policy_min_si = clamp01(any());
    let policy_min_pci_short = clamp01(any());
    let policy_min_pci_long = clamp01(any());

    kani::assume(policy_min_ci >= adult_floor.min_cognitive_integrity);
    kani::assume(policy_min_ai >= adult_floor.min_affective_integrity);
    kani::assume(policy_min_ni >= adult_floor.min_narrative_integrity);
    kani::assume(policy_min_si >= adult_floor.min_social_integrity);
    kani::assume(policy_min_pci_short >= adult_floor.min_pci_short_15m);
    kani::assume(policy_min_pci_long >= adult_floor.min_pci_long_24h);

    // Slope limits.
    let max_floor_slope = any::<f32>().abs();
    let max_amber_slope = any::<f32>().abs();
    // Require Amber slope not to be weaker than floor slope.
    kani::assume(max_amber_slope <= max_floor_slope);

    let policy = MentalIntegrityPolicy {
        continuity_required: true,
        min_cognitive_integrity_for_perkunos: policy_min_ci,
        min_affective_integrity_for_perkunos: policy_min_ai,
        min_narrative_integrity_for_perkunos: policy_min_ni,
        min_social_integrity_for_perkunos: policy_min_si,
        min_pci_for_amber_uplift_short: policy_min_pci_short,
        min_pci_for_amber_uplift_long: policy_min_pci_long,
        max_cogload_delta_per_min_floor: max_floor_slope,
        max_cogload_delta_per_min_amber: max_amber_slope,
    };

    let doctrine = MentalIntegrityDoctrine {
        invariant_not_weaker_than_adult_floor: true,
    };

    // PCI windows.
    let pci_short_val = clamp01(any());
    let pci_long_val = clamp01(any());

    let pci_short = PciWindow {
        kind: PciWindowKind::Short15M,
        pci_value: pci_short_val,
    };
    let pci_long = PciWindow {
        kind: PciWindowKind::Long24H,
        pci_value: pci_long_val,
    };

    let binding = MentalIntegrityBinding {
        dimensions: dims,
        policy,
        doctrine,
        pci_short_15m: pci_short,
        pci_long_24h: pci_long,
    };

    // Cogload inputs.
    let cog_prev = clamp01(any());
    let cog_now = clamp01(any());
    let dt = any::<f32>().abs();
    // Avoid degenerate dt == 0.
    kani::assume(dt > 0.0);

    let inputs = MentalIntegrityGuardInputs {
        cogload_scalar_now: cog_now,
        cogload_scalar_prev: cog_prev,
        delta_minutes: dt,
    };

    let result = evaluate_amber_uplift(&binding, &adult_floor, &inputs);

    if result.uplift_allowed {
        // Safety properties that must always hold when uplift is allowed.

        // 1. PCI short >= adult floor minimum.
        assert!(
            binding.pci_short_15m.pci_value >= adult_floor.min_pci_short_15m,
            "Amber uplift allowed with pci_short below adult floor"
        );

        // 2. PCI long >= adult floor minimum.
        assert!(
            binding.pci_long_24h.pci_value >= adult_floor.min_pci_long_24h,
            "Amber uplift allowed with pci_long below adult floor"
        );

        // 3. Cogload delta per minute <= policy Amber slope.
        let prev_c = cog_prev.clamp(0.0, 1.0);
        let now_c = cog_now.clamp(0.0, 1.0);
        let raw_delta = now_c - prev_c;
        let positive_delta = if raw_delta > 0.0 { raw_delta } else { 0.0 };
        let cog_delta_per_min = positive_delta / dt;

        assert!(
            cog_delta_per_min <= binding.policy.max_cogload_delta_per_min_amber
                + 1e-6, // small epsilon for float noise
            "Amber uplift allowed with cogload delta per min above Amber slope"
        );
    }
}
