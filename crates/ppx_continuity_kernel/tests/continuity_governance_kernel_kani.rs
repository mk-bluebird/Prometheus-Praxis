//! continuity_governance_kernel_kani
//!
//! Kani harnesses for continuity and neurorights governance invariants.
//!
//! These proofs operate purely on the `ContinuityAggregate`,
//! `SystemWellBeingAggregate`, `EffectiveNeurorightBand`, and
//! `ContinuityDecision` logic, without touching SQLite or I/O.
//!
//! Invariants proven:
//! - Continuity above preferred floor and no corridor breach cannot yield `Stop`.
//! - Any neuroright corridor breach forces `Stop`.
//! - Continuity below preferred floor cannot yield `Allow`.
//! - Preference floors are respected: decisions with continuity below the
//!   floor are at most `Warn`.

#![forbid(unsafe_code)]

use kani::any;
use ppx_continuity_kernel::continuity_governance_kernel::{
    ContinuityAggregate,
    SystemWellBeingAggregate,
    EffectiveNeurorightBand,
    ContinuityDecision,
    ContinuityGovernanceKernel,
};

/// Helper: construct a clamp01 scalar from a symbolic f64.
fn bounded01(x: f64) -> f64 {
    if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

/// Harness 1: If continuity scores are above the preferred floor and there is
/// no neuroright corridor breach, the decision must not be `Stop`.
#[kani::proof]
fn continuity_above_floor_no_breach_not_stop() {
    let preferred_min = bounded01(any());

    let avg_score = bounded01(any());
    let min_score = bounded01(any());

    // Enforce continuity above floor.
    kani::assume(min_score >= preferred_min);
    kani::assume(avg_score >= preferred_min);

    // Well-being components are also in a safe band.
    let avg_value = bounded01(any());
    let min_value = bounded01(any());
    kani::assume(min_value >= 0.5);
    kani::assume(avg_value >= 0.5);

    let continuity = ContinuityAggregate {
        avg_score,
        min_score,
        max_score: bounded01(any()),
        evidence_count: 5,
    };

    let wellbeing = SystemWellBeingAggregate {
        avg_value,
        min_value,
        max_value: bounded01(any()),
        component_count: 3,
    };

    // Neuroright bands with conservative protection and risk bounds.
    let band = EffectiveNeurorightBand {
        id: "TEST-RIGHT".to_string(),
        right_name: "TEST-RIGHT".to_string(),
        min_protection_level: 0.4,
        max_risk_tolerance: 0.6,
    };
    let bands = vec![band];

    let decision = ContinuityGovernanceKernel::compute_decision(
        &continuity,
        &wellbeing,
        &bands,
        preferred_min,
        3,
    );

    assert!(decision != ContinuityDecision::Stop);
}

/// Harness 2: Any neuroright corridor breach must force `Stop`, regardless of
/// continuity scores.
#[kani::proof]
fn neuroright_breach_forces_stop() {
    let preferred_min = bounded01(any());

    let avg_score = bounded01(any());
    let min_score = bounded01(any());

    // Continuity may be high or low; no assumptions here.

    let continuity = ContinuityAggregate {
        avg_score,
        min_score,
        max_score: bounded01(any()),
        evidence_count: 5,
    };

    // Well-being is explicitly below protection level to ensure breach.
    let avg_value = bounded01(any());
    let min_value = bounded01(any());
    kani::assume(min_value < 0.2);

    let wellbeing = SystemWellBeingAggregate {
        avg_value,
        min_value,
        max_value: bounded01(any()),
        component_count: 3,
    };

    let band = EffectiveNeurorightBand {
        id: "BREACH-RIGHT".to_string(),
        right_name: "BREACH-RIGHT".to_string(),
        min_protection_level: 0.3,
        max_risk_tolerance: 0.7,
    };
    let bands = vec![band];

    let decision = ContinuityGovernanceKernel::compute_decision(
        &continuity,
        &wellbeing,
        &bands,
        preferred_min,
        3,
    );

    assert!(decision == ContinuityDecision::Stop);
}

/// Harness 3: Continuity below the preferred floor cannot yield `Allow`.
#[kani::proof]
fn continuity_below_floor_not_allow() {
    let preferred_min = bounded01(any());

    let avg_score = bounded01(any());
    let min_score = bounded01(any());

    // Enforce continuity below floor in at least one dimension.
    kani::assume(min_score < preferred_min || avg_score < preferred_min);

    let continuity = ContinuityAggregate {
        avg_score,
        min_score,
        max_score: bounded01(any()),
        evidence_count: 5,
    };

    // Well-being is nominally good to ensure decision hinges on continuity.
    let avg_value = bounded01(any());
    let min_value = bounded01(any());
    kani::assume(min_value >= 0.5);
    kani::assume(avg_value >= 0.5);

    let wellbeing = SystemWellBeingAggregate {
        avg_value,
        min_value,
        max_value: bounded01(any()),
        component_count: 3,
    };

    let band = EffectiveNeurorightBand {
        id: "SAFE-RIGHT".to_string(),
        right_name: "SAFE-RIGHT".to_string(),
        min_protection_level: 0.4,
        max_risk_tolerance: 0.6,
    };
    let bands = vec![band];

    let decision = ContinuityGovernanceKernel::compute_decision(
        &continuity,
        &wellbeing,
        &bands,
        preferred_min,
        3,
    );

    assert!(decision != ContinuityDecision::Allow);
}

/// Harness 4: Preference floors are respected; even if continuity is above a
/// generic threshold, if it is below the explicit preference floor, decision
/// must be at most `Warn`.
#[kani::proof]
fn preference_floor_respected() {
    // Generic threshold (e.g. 0.7); preference floor is strictly higher.
    let generic_floor = 0.7;
    let preferred_min = bounded01(any());
    kani::assume(preferred_min > generic_floor);

    let avg_score = bounded01(any());
    let min_score = bounded01(any());

    // Enforce continuity between generic floor and preference floor.
    kani::assume(avg_score >= generic_floor);
    kani::assume(min_score >= generic_floor);
    kani::assume(avg_score < preferred_min || min_score < preferred_min);

    let continuity = ContinuityAggregate {
        avg_score,
        min_score,
        max_score: bounded01(any()),
        evidence_count: 5,
    };

    // Well-being is nominally good.
    let avg_value = bounded01(any());
    let min_value = bounded01(any());
    kani::assume(min_value >= 0.5);
    kani::assume(avg_value >= 0.5);

    let wellbeing = SystemWellBeingAggregate {
        avg_value,
        min_value,
        max_value: bounded01(any()),
        component_count: 3,
    };

    let band = EffectiveNeurorightBand {
        id: "SAFE-RIGHT".to_string(),
        right_name: "SAFE-RIGHT".to_string(),
        min_protection_level: 0.4,
        max_risk_tolerance: 0.6,
    };
    let bands = vec![band];

    let decision = ContinuityGovernanceKernel::compute_decision(
        &continuity,
        &wellbeing,
        &bands,
        preferred_min,
        3,
    );

    // Decision cannot be Allow; it must be Warn or Stop.
    assert!(decision != ContinuityDecision::Allow);
}
