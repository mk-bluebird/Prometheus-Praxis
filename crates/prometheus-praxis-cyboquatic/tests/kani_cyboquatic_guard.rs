#![allow(dead_code)]
#![forbid(unsafe_code)]

use prometheus_praxis_cyboquatic::{
    CyboquaticDecision, CyboquaticEcoEvidenceSummary, CyboquaticInputs, CyboquaticScore,
    compute_cyboquatic_score, evaluate_cyboquatic_decision, safety_envelopes_ok,
    W_DATA_LABOR, W_KER, W_LIFE, W_SAFETY,
};
use prometheus_praxis_lyapunov_guard::{
    KerSnapshot, ROH_CEILING_GLOBAL, LIFEFORCE_FLOOR_GLOBAL, BCR_MIN_GLOBAL,
    PAIN_INDEX_CEILING_GLOBAL, FEAR_INDEX_MIN_GLOBAL, FEAR_INDEX_MAX_GLOBAL,
};

fn safe_ker_snapshot() -> KerSnapshot {
    KerSnapshot {
        carbon_removal: 0.0,
        water_restoration: 0.0,
        biodiversity_gain: 0.0,
        toxicity_reduction: 0.0,
        socio_ecolabour: 0.5,
        rohscalar: ROH_CEILING_GLOBAL,
        lifeforcescalar: LIFORCEFLOOR_GLOBAL,
        biocompatibilityrating: BCR_MIN_GLOBAL,
        painindex: PAIN_INDEX_CEILING_GLOBAL,
        fearindex: (FEAR_INDEX_MIN_GLOBAL + FEAR_INDEX_MAX_GLOBAL) / 2.0,
    }
}

/// Harness 1: safety_envelopes_ok agrees with Lyapunov guard envelopes.
#[kani::proof]
fn kani_cybo_safety_envelopes_ok_at_boundaries() {
    let snap = safe_ker_snapshot();
    assert!(safety_envelopes_ok(&snap));
}

/// Harness 2: compute_cyboquatic_score respects [0,1] range and weights sum.
///
/// This checks the basic numeric sanity of the index.
#[kani::proof]
fn kani_cyboquatic_score_in_range() {
    let lifeforce_conservation: f32 = kani::any();
    let eco_ker_norm: f32 = kani::any();
    let psych_safety_norm: f32 = kani::any();
    let data_labor_density: f32 = kani::any();

    let inputs = CyboquaticInputs {
        ker_snapshot: safe_ker_snapshot(),
        lifeforce_conservation,
        eco_ker_norm,
        psych_safety_norm,
        data_labor_density,
    };

    let score = compute_cyboquatic_score(&inputs);

    // Score components should be non-negative, index_value within [0,1] by construction.
    assert!(score.lifeforce_component >= 0.0);
    assert!(score.ker_component >= 0.0);
    assert!(score.safety_component >= 0.0);
    assert!(score.data_labor_component >= 0.0);
    assert!(score.index_value >= 0.0);
    assert!(score.index_value <= 1.0);

    let weights_sum = W_LIFE + W_KER + W_SAFETY + W_DATA_LABOR;
    assert!((weights_sum - 1.0).abs() < 1e-6);
}

/// Harness 3: if index increases and no data-labor, decision must RejectNoDataLabor.
#[kani::proof]
fn kani_cyboquatic_requires_data_labor_for_increase() {
    let base_inputs = CyboquaticInputs {
        ker_snapshot: safe_ker_snapshot(),
        lifeforce_conservation: 0.5,
        eco_ker_norm: 0.5,
        psych_safety_norm: 0.5,
        data_labor_density: 0.0,
    };
    let before_score = compute_cyboquatic_score(&base_inputs);

    let improved_inputs = CyboquaticInputs {
        lifeforce_conservation: 0.9,
        eco_ker_norm: 0.9,
        psych_safety_norm: 0.9,
        data_labor_density: 0.0, // still no data-labor
        ..base_inputs
    };
    let after_score = compute_cyboquatic_score(&improved_inputs);

    let eco_evidence = CyboquaticEcoEvidenceSummary {
        evidence_ids: Vec::new(),
        has_measurement_tethered: false,
    };

    let decision = evaluate_cyboquatic_decision(
        &before_score,
        &after_score,
        &base_inputs.ker_snapshot,
        &improved_inputs.ker_snapshot,
        &eco_evidence,
    );

    if after_score.index_value > before_score.index_value {
        assert!(matches!(decision, CyboquaticDecision::RejectNoDataLabor));
    }
}

/// Harness 4: if safety envelopes violated, decision must RejectSafety.
#[kani::proof]
fn kani_cyboquatic_rejects_safety_violation() {
    let mut unsafe_snapshot = safe_ker_snapshot();
    unsafe_snapshot.rohscalar = ROH_CEILING_GLOBAL + 0.1;

    let before_score = CyboquaticScore {
        index_value: 0.5,
        lifeforce_component: 0.0,
        ker_component: 0.0,
        safety_component: 0.0,
        data_labor_component: 0.0,
    };

    let inputs = CyboquaticInputs {
        ker_snapshot: unsafe_snapshot.clone(),
        lifeforce_conservation: 0.5,
        eco_ker_norm: 0.5,
        psych_safety_norm: 0.5,
        data_labor_density: 1.0,
    };
    let after_score = compute_cyboquatic_score(&inputs);

    let eco_evidence = CyboquaticEcoEvidenceSummary {
        evidence_ids: vec!["eco-event-1".to_string()],
        has_measurement_tethered: true,
    };

    let decision = evaluate_cyboquatic_decision(
        &before_score,
        &after_score,
        &safe_ker_snapshot(),
        &unsafe_snapshot,
        &eco_evidence,
    );

    assert!(matches!(decision, CyboquaticDecision::RejectSafety));
}
