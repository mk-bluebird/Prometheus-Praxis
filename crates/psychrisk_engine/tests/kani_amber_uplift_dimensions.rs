// filepath: crates/psychrisk_engine/tests/kani_amber_uplift_dimensions.rs
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

/// Kani verification harness for the mental integrity dimension invariant:
///
/// uplift_allowed == true ⇒
///   cognitive_integrity >= policy.min_cognitive_integrity_for_perkunos ∧
///   cognitive_integrity >= adult_floor.min_cognitive_integrity ∧
///   affective_integrity >= policy.min_affective_integrity_for_perkunos ∧
///   affective_integrity >= adult_floor.min_affective_integrity ∧
///   narrative_integrity >= policy.min_narrative_integrity_for_perkunos ∧
///   narrative_integrity >= adult_floor.min_narrative_integrity ∧
///   social_integrity >= policy.min_social_integrity_for_perkunos ∧
///   social_integrity >= adult_floor.min_social_integrity.
#[kani::proof]
fn verify_amber_uplift_respects_dimension_floors() {
    // Arbitrary mental integrity dimensions (clamped).
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

    // Adult floor thresholds.
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

    // Policy thresholds, constrained to be not weaker than the adult floor.
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

    // Slope limits (non‑negative).
    let max_floor_slope = any::<f32>().abs();
    let max_amber_slope = any::<f32>().abs();
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

    // PCI windows (values arbitrary but clamped).
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
        dimensions: dims.clone(),
        policy,
        doctrine,
        pci_short_15m: pci_short,
        pci_long_24h: pci_long,
    };

    // Cogload inputs (arbitrary, clamped).
    let cog_prev = clamp01(any());
    let cog_now = clamp01(any());
    let dt = any::<f32>().abs();
    kani::assume(dt > 0.0);

    let inputs = MentalIntegrityGuardInputs {
        cogload_scalar_now: cog_now,
        cogload_scalar_prev: cog_prev,
        delta_minutes: dt,
    };

    let result = evaluate_amber_uplift(&binding, &adult_floor, &inputs);

    if result.uplift_allowed {
        // Cognitive integrity floors.
        assert!(
            dims.cognitive_integrity >= binding.policy.min_cognitive_integrity_for_perkunos,
            "Amber uplift allowed with cognitive_integrity below policy floor"
        );
        assert!(
            dims.cognitive_integrity >= adult_floor.min_cognitive_integrity,
            "Amber uplift allowed with cognitive_integrity below adult floor"
        );

        // Affective integrity floors.
        assert!(
            dims.affective_integrity >= binding.policy.min_affective_integrity_for_perkunos,
            "Amber uplift allowed with affective_integrity below policy floor"
        );
        assert!(
            dims.affective_integrity >= adult_floor.min_affective_integrity,
            "Amber uplift allowed with affective_integrity below adult floor"
        );

        // Narrative integrity floors.
        assert!(
            dims.narrative_integrity >= binding.policy.min_narrative_integrity_for_perkunos,
            "Amber uplift allowed with narrative_integrity below policy floor"
        );
        assert!(
            dims.narrative_integrity >= adult_floor.min_narrative_integrity,
            "Amber uplift allowed with narrative_integrity below adult floor"
        );

        // Social integrity floors.
        assert!(
            dims.social_integrity >= binding.policy.min_social_integrity_for_perkunos,
            "Amber uplift allowed with social_integrity below policy floor"
        );
        assert!(
            dims.social_integrity >= adult_floor.min_social_integrity,
            "Amber uplift allowed with social_integrity below adult floor"
        );
    }
}
