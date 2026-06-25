// filepath: eco_restoration_shard/cybercore/psychrisk_engine/src/mental_integrity_guard.rs
#![forbid(unsafe_code)]

use crate::types::{
    AmberUpliftDecision,
    MentalIntegrityBinding,
    MentalIntegrityDimensions,
    MentalIntegrityPolicy,
    PciWindow,
    PciWindowKind,
};

/// Result of an Amber uplift evaluation.
///
/// This is deliberately simple and side‑effect free so that Kani/Prusti can
/// verify invariants over all execution paths.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct AmberUpliftDecisionResult {
    pub uplift_allowed: bool,
    pub reason: AmberUpliftReason,
}

/// Enumerates the reasons why Amber uplift was allowed or denied.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum AmberUpliftReason {
    AllowedAllInvariantsSatisfied,
    DeniedContinuityNotRequired,
    DeniedPolicyNotNonWeakerThanAdultFloor,
    DeniedPciShortBelowThreshold,
    DeniedPciLongBelowThreshold,
    DeniedCognitiveIntegrityBelowFloor,
    DeniedAffectiveIntegrityBelowFloor,
    DeniedNarrativeIntegrityBelowFloor,
    DeniedSocialIntegrityBelowFloor,
    DeniedCogloadDeltaExceedsAmberSlope,
    DeniedMalformedInputs,
}

/// A minimal view of the time‑local psychrisk data the guard needs.
///
/// This keeps the guard independent of the full PsychRiskEventV1 payload while
/// still allowing it to reason about cogload slope and current risk band.
#[derive(Clone, Debug)]
pub struct MentalIntegrityGuardInputs {
    /// Current instantaneous cognitive load scalar in [0.0, 1.0].
    pub cogload_scalar_now: f32,
    /// Previous instantaneous cognitive load scalar in [0.0, 1.0].
    pub cogload_scalar_prev: f32,
    /// Time delta between successive samples in minutes (> 0.0).
    pub delta_minutes: f32,
}

/// A minimal view of the adult floor envelope relevant to this guard.
///
/// The full BioSafetyEnvelopeV1 contains more, but the guard only needs the
/// adult floor thresholds to enforce the non‑weakening doctrine.
#[derive(Clone, Debug)]
pub struct AdultFloorEnvelope {
    pub min_cognitive_integrity: f32,
    pub min_affective_integrity: f32,
    pub min_narrative_integrity: f32,
    pub min_social_integrity: f32,
    pub min_pci_short_15m: f32,
    pub min_pci_long_24h: f32,
}

/// A pure, side‑effect free guard that decides whether Amber uplift is allowed.
///
/// It reads:
///   - The mental.integrity.binding.v1 binding (policy + PCI windows + dims),
///   - The adult floor envelope, and
///   - The current cogload slope,
/// and returns an AmberUpliftDecisionResult. The decision carries all the
/// information needed by perkunos_nexus_guard.rs without mutating any host state.
///
/// KEY INVARIANTS (for formal verification):
///   1. Non‑weakening adult floor:
///      If uplift_allowed == true, then all mental integrity dimensions and PCI
///      values are >= their corresponding adult floor thresholds.
///   2. Cogload slope safety:
///      If uplift_allowed == true, then cogload_delta_per_min <=
///      max_cogload_delta_per_min_amber AND also <=
///      max_cogload_delta_per_min_floor.
///   3. Fail‑closed on malformed input:
///      Any malformed inputs (NaN, non‑positive delta_minutes) result in
///      uplift_allowed == false.
pub fn evaluate_amber_uplift(
    binding: &MentalIntegrityBinding,
    adult_floor: &AdultFloorEnvelope,
    inputs: &MentalIntegrityGuardInputs,
) -> AmberUpliftDecisionResult {
    // Fail‑closed on malformed inputs.
    if !are_inputs_well_formed(inputs) {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedMalformedInputs,
        };
    }

    let policy: &MentalIntegrityPolicy = &binding.policy;
    let dims: &MentalIntegrityDimensions = &binding.dimensions;

    // If the policy does not require continuity, Amber uplift is forbidden.
    if !policy.continuity_required {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedContinuityNotRequired,
        };
    }

    // Enforce global doctrine: policy must not be weaker than adult floor.
    if !binding.doctrine.invariant_not_weaker_than_adult_floor {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedPolicyNotNonWeakerThanAdultFloor,
        };
    }

    // Short‑ and long‑horizon PCI windows for this binding.
    let pci_short = select_pci_window(&binding.pci_short_15m, PciWindowKind::Short15M);
    let pci_long = select_pci_window(&binding.pci_long_24h, PciWindowKind::Long24H);

    // 1. PCI thresholds (short window).
    if pci_short.pci_value < policy.min_pci_for_amber_uplift_short
        || pci_short.pci_value < adult_floor.min_pci_short_15m
    {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedPciShortBelowThreshold,
        };
    }

    // 2. PCI thresholds (long window).
    if pci_long.pci_value < policy.min_pci_for_amber_uplift_long
        || pci_long.pci_value < adult_floor.min_pci_long_24h
    {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedPciLongBelowThreshold,
        };
    }

    // 3. Mental integrity dimensions must be >= both policy and adult floor.
    if dims.cognitive_integrity < policy.min_cognitive_integrity_for_perkunos
        || dims.cognitive_integrity < adult_floor.min_cognitive_integrity
    {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedCognitiveIntegrityBelowFloor,
        };
    }

    if dims.affective_integrity < policy.min_affective_integrity_for_perkunos
        || dims.affective_integrity < adult_floor.min_affective_integrity
    {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedAffectiveIntegrityBelowFloor,
        };
    }

    if dims.narrative_integrity < policy.min_narrative_integrity_for_perkunos
        || dims.narrative_integrity < adult_floor.min_narrative_integrity
    {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedNarrativeIntegrityBelowFloor,
        };
    }

    if dims.social_integrity < policy.min_social_integrity_for_perkunos
        || dims.social_integrity < adult_floor.min_social_integrity
    {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedSocialIntegrityBelowFloor,
        };
    }

    // 4. Cogload slope constraint (per minute), respect both policy Amber slope
    //    and adult floor slope, and keep it non‑negative.
    let cogload_delta_per_min = compute_cogload_delta_per_min(
        inputs.cogload_scalar_prev,
        inputs.cogload_scalar_now,
        inputs.delta_minutes,
    );

    if cogload_delta_per_min > policy.max_cogload_delta_per_min_amber
        || cogload_delta_per_min > policy.max_cogload_delta_per_min_floor
    {
        return AmberUpliftDecisionResult {
            uplift_allowed: false,
            reason: AmberUpliftReason::DeniedCogloadDeltaExceedsAmberSlope,
        };
    }

    // All invariants satisfied: Amber uplift is allowed.
    AmberUpliftDecisionResult {
        uplift_allowed: true,
        reason: AmberUpliftReason::AllowedAllInvariantsSatisfied,
    }
}

/// Simple structural check that inputs are not NaN and delta_minutes > 0.
///
/// This function is a good target for a small Prusti/Kani proof: for all
/// finite inputs, it returns false only in malformed cases.
fn are_inputs_well_formed(inputs: &MentalIntegrityGuardInputs) -> bool {
    let c_now = inputs.cogload_scalar_now;
    let c_prev = inputs.cogload_scalar_prev;
    let dt = inputs.delta_minutes;

    // Reject NaNs or non‑positive dt.
    if !c_now.is_finite() || !c_prev.is_finite() || !dt.is_finite() || dt <= 0.0 {
        return false;
    }

    // Clamp‑style semantics are handled in compute_cogload_delta_per_min; here
    // we just ensure the values are not wildly malformed.
    true
}

/// Compute cogload slope per minute from two clamped scalars.
///
/// The result is always >= 0.0, because we only care about how fast load is
/// increasing for safety. This makes the safety property monotone and easy
/// to reason about formally.
fn compute_cogload_delta_per_min(prev: f32, now: f32, delta_minutes: f32) -> f32 {
    let prev_c = clamp01(prev);
    let now_c = clamp01(now);
    let raw_delta = now_c - prev_c;
    let positive_delta = if raw_delta > 0.0 { raw_delta } else { 0.0 };
    positive_delta / delta_minutes
}

/// Clamp a scalar into [0.0, 1.0], robust to minor out‑of‑range noise.
fn clamp01(x: f32) -> f32 {
    if x.is_nan() {
        0.0
    } else if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

/// Select the PCI window with the requested kind.
///
/// The binding is assumed to already hold the correct window; this function
/// exists mainly to give Kani/Prusti a simple, total function to reason about.
fn select_pci_window(window: &PciWindow, kind: PciWindowKind) -> PciWindow {
    // For now, there is exactly one window per kind on the binding, so we just
    // return a clone. If you later support multiple, this is the place to
    // implement the selection rule.
    match kind {
        PciWindowKind::Short15M => window.clone(),
        PciWindowKind::Long24H => window.clone(),
    }
}

// Optional: glue function that converts this guards decision into the
// higher‑level AmberUpliftDecision type exposed to perkunos_nexus_guard.rs.

impl From<AmberUpliftDecisionResult> for AmberUpliftDecision {
    fn from(result: AmberUpliftDecisionResult) -> Self {
        AmberUpliftDecision {
            uplift_allowed: result.uplift_allowed,
            reason: format!("{:?}", result.reason),
        }
    }
}
