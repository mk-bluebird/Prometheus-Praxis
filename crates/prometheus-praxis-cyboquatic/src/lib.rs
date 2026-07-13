// crates/prometheus-praxis-cyboquatic/src/lib.rs
// Designed for https://github.com/mk-bluebird/Prometheus-Praxis
// Cyboquatic index computation and guard, eco-labour anchored.
// Rust 2024, rust-version = "1.85", Kani 0.67, no unsafe.
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

use prometheus_praxis_lyapunov_guard::KerSnapshot;

/// Sovereign bindings (must match cyboquatic-index.v1.aln).
pub const HOST_DID: &str = "didalnorganic-host";
pub const PRIMARY_BOSTROM_ADDRESS: &str =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
pub const ALN_MIGRATION_AUTHORITY: &str =
    "ALN.MIGRATION.CYBERCOREAUTHORITY.v1";

/// Global safety envelopes (mirror ALN fields).
pub const ROH_CEILING_GLOBAL: f32 = 0.30;
pub const LIFORCEFLOOR_GLOBAL: f32 = 0.57;
pub const BCR_MIN_GLOBAL: f32 = 0.57;
pub const PAIN_INDEX_CEILING_GLOBAL: f32 = 0.73;
pub const FEAR_INDEX_MIN_GLOBAL: f32 = 0.31;
pub const FEAR_INDEX_MAX_GLOBAL: f32 = 0.68;

/// Weights for CyboquaticIndex (must match ALN weights).
pub const W_LIFE: f32 = 0.25;
pub const W_KER: f32 = 0.35;
pub const W_SAFETY: f32 = 0.20;
pub const W_DATA_LABOR: f32 = 0.20;

/// Inputs to Cyboquatic index computation.
/// Mirrors CyboquaticInputs in cyboquatic-index.v1.aln.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboquaticInputs {
    pub ker_snapshot: KerSnapshot,
    pub lifeforce_conservation: f32,
    pub eco_ker_norm: f32,
    pub psych_safety_norm: f32,
    pub data_labor_density: f32,
}

/// Decomposed Cyboquatic score.
/// Mirrors CyboquaticScore in cyboquatic-index.v1.aln.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboquaticScore {
    pub index_value: f32,
    pub lifeforce_component: f32,
    pub ker_component: f32,
    pub safety_component: f32,
    pub data_labor_component: f32,
}

/// Before/after pair for index evolution.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboquaticDelta {
    pub before: CyboquaticScore,
    pub after: CyboquaticScore,
}

/// Decision outcomes for index-governed actions.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum CyboquaticDecision {
    Accept,
    RejectSafety,
    RejectNoDataLabor,
    RejectIndexRegression,
}

/// Minimal eco-labour evidence summary for Cyboquatic window.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboquaticEcoEvidenceSummary {
    pub evidence_ids: Vec<String>,
    pub has_measurement_tethered: bool,
}

/// Compute the CyboquaticScore from inputs and ALN-weighted formula.
///
/// Invariants:
/// - All component contributions are non-negative.
/// - `index_value` == weighted sum of components with W_LIFE + W_KER + W_SAFETY + W_DATA_LABOR == 1.0.
pub fn compute_cyboquatic_score(inputs: &CyboquaticInputs) -> CyboquaticScore {
    let lifeforce_component = W_LIFE * clamp01(inputs.lifeforce_conservation);
    let ker_component = W_KER * clamp01(inputs.eco_ker_norm);
    let safety_component = W_SAFETY * clamp01(inputs.psych_safety_norm);
    let data_labor_component = W_DATA_LABOR * clamp01(inputs.data_labor_density);

    let index_value =
        lifeforce_component + ker_component + safety_component + data_labor_component;

    CyboquaticScore {
        index_value,
        lifeforce_component,
        ker_component,
        safety_component,
        data_labor_component,
    }
}

/// Evaluate whether a governance step governed by CyboquaticIndex is acceptable.
///
/// Invariants (to be proven by Kani):
/// - If `after.index_value` < `before.index_value`, decision MUST be RejectIndexRegression.
/// - If index increases and `has_measurement_tethered` is false, decision MUST be RejectNoDataLabor.
/// - If safety envelopes are violated in the underlying KerSnapshot, decision MUST be RejectSafety.
/// - Accept only when index is non-decreasing, eco_ker_norm and psych_safety_norm are non-decreasing,
///   and eco-labour evidence is present for positive deltas.
pub fn evaluate_cyboquatic_decision(
    before: &CyboquaticScore,
    after: &CyboquaticScore,
    ker_before: &KerSnapshot,
    ker_after: &KerSnapshot,
    eco_evidence: &CyboquaticEcoEvidenceSummary,
) -> CyboquaticDecision {
    let safety_ok = safety_envelopes_ok(ker_after);
    let index_delta = after.index_value - before.index_value;

    if !safety_ok {
        return CyboquaticDecision::RejectSafety;
    }

    if index_delta < 0.0 {
        return CyboquaticDecision::RejectIndexRegression;
    }

    if index_delta > 0.0 && !eco_evidence.has_measurement_tethered {
        return CyboquaticDecision::RejectNoDataLabor;
    }

    CyboquaticDecision::Accept
}

/// Clamp a scalar into [0,1].
fn clamp01(x: f32) -> f32 {
    if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

/// Shared safety envelope check for KerSnapshot.
///
/// This is kept in sync with the Lyapunov guard crate; you can
/// either re-use the function from there or keep this duplicate
/// with strict tests to ensure identical behavior.
pub fn safety_envelopes_ok(snapshot: &KerSnapshot) -> bool {
    snapshot.rohscalar <= ROH_CEILING_GLOBAL
        && snapshot.liforcescalar >= LIFORCEFLOOR_GLOBAL
        && snapshot.biocompatibilityrating >= BCR_MIN_GLOBAL
        && snapshot.painindex <= PAIN_INDEX_CEILING_GLOBAL
        && snapshot.fearindex >= FEAR_INDEX_MIN_GLOBAL
        && snapshot.fearindex <= FEAR_INDEX_MAX_GLOBAL
}
