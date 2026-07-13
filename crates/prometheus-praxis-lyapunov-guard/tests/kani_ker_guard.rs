#![allow(dead_code)]
#![forbid(unsafe_code)]

use prometheus_praxis_lyapunov_guard::{
    EcoLaborEvidenceSummary, KerDelta, KerGuardDecision, KerSnapshot, LyapunovResidual,
    ROH_CEILING_GLOBAL, LIFEFORCE_FLOOR_GLOBAL, BCR_MIN_GLOBAL, PAIN_INDEX_CEILING_GLOBAL,
    FEAR_INDEX_MIN_GLOBAL, FEAR_INDEX_MAX_GLOBAL, compute_v_eco, evaluate_ker_guard,
    safety_envelopes_ok,
};

/// Helper: construct a KerSnapshot within global safety envelopes.
fn safe_snapshot() -> KerSnapshot {
    KerSnapshot {
        carbon_removal: 0.0,
        water_restoration: 0.0,
        biodiversity_gain: 0.0,
        toxicity_reduction: 0.0,
        socio_ecolabour: 0.5,
        rohscalar: ROH_CEILING_GLOBAL,
        lifeforcescalar: LIFEFORCE_FLOOR_GLOBAL,
        biocompatibilityrating: BCR_MIN_GLOBAL,
        painindex: PAIN_INDEX_CEILING_GLOBAL,
        fearindex: (FEAR_INDEX_MIN_GLOBAL + FEAR_INDEX_MAX_GLOBAL) / 2.0,
    }
}

/// Harness 1: safety_envelopes_ok is true for a snapshot at exact envelope boundaries.
#[kani::proof]
fn kani_safety_envelopes_ok_at_boundaries() {
    let snap = safe_snapshot();
    assert!(safety_envelopes_ok(&snap));
}

/// Harness 2: if safety envelopes are violated, evaluate_ker_guard must RejectSafety.
#[kani::proof]
fn kani_guard_rejects_safety_violation() {
    let before = safe_snapshot();
    let mut after = safe_snapshot();
    // Violate RoH ceiling.
    after.rohscalar = ROH_CEILING_GLOBAL + 0.1;

    let delta = KerDelta { before, after };

    let eco_evidence = EcoLaborEvidenceSummary {
        evidence_ids: vec!["eco-event-1".to_string()],
        has_measurement_tethered: true,
    };

    let (decision, _residual) = evaluate_ker_guard(&delta, &eco_evidence);
    assert!(matches!(decision, KerGuardDecision::RejectSafety));
}

/// Harness 3: if V_after > V_before, evaluate_ker_guard must RejectNonMonotone.
///
/// This assumes compute_v_eco is monotone in socio_ecolabour or at least
/// not trivially constant; if currently constant, this harness will still
/// check that residual > 0 implies RejectNonMonotone.
#[kani::proof]
fn kani_guard_rejects_non_monotone() {
    let mut before = safe_snapshot();
    let mut after = safe_snapshot();

    // Let Kani explore any socio_ecolabour delta.
    let dv: f32 = kani::any();
    after.socio_ecolabour = before.socio_ecolabour + dv;

    let delta = KerDelta { before, after };
    let eco_evidence = EcoLaborEvidenceSummary {
        evidence_ids: vec!["eco-event-1".to_string()],
        has_measurement_tethered: true,
    };

    let (decision, LyapunovResidual { residual, .. }) =
        evaluate_ker_guard(&delta, &eco_evidence);

    if residual > 0.0 {
        assert!(matches!(decision, KerGuardDecision::RejectNonMonotone));
    }
}

/// Harness 4: if V_after < V_before and no eco-labour evidence,
/// evaluate_ker_guard must RejectDataLaborMissing.
#[kani::proof]
fn kani_guard_requires_data_labor_for_improvement() {
    let before = safe_snapshot();
    let mut after = safe_snapshot();

    // Let Kani choose a change that might improve V_eco.
    let dv: f32 = kani::any();
    after.socio_ecolabour = before.socio_ecolabour + dv;

    let delta = KerDelta { before, after };

    let eco_evidence = EcoLaborEvidenceSummary {
        evidence_ids: Vec::new(),
        has_measurement_tethered: false,
    };

    let (decision, residual) = evaluate_ker_guard(&delta, &eco_evidence);

    if residual.residual < 0.0 {
        assert!(matches!(decision, KerGuardDecision::RejectDataLaborMissing));
    }
}
