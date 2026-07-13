// crates/prometheus-praxis-lyapunov-guard/src/lib.rs
// Designed for https://github.com/mk-bluebird/Prometheus-Praxis
// Ecosystem KER Lyapunov guard for eco-labour–anchored evolution.
// Rust 2024, rust-version = "1.85", Kani 0.67, no unsafe.
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Sovereign bindings (must match ALN ecosystem-ker-profile.v1.aln).
pub const HOST_DID: &str = "didalnorganic-host";
pub const PRIMARY_BOSTROM_ADDRESS: &str =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
pub const ALN_MIGRATION_AUTHORITY: &str =
    "ALN.MIGRATION.CYBERCOREAUTHORITY.v1";

/// Global safety envelopes (mirror ALN fields).
pub const ROH_CEILING_GLOBAL: f32 = 0.30;
pub const LIFEFORCE_FLOOR_GLOBAL: f32 = 0.57;
pub const BCR_MIN_GLOBAL: f32 = 0.57;
pub const PAIN_INDEX_CEILING_GLOBAL: f32 = 0.73;
pub const FEAR_INDEX_MIN_GLOBAL: f32 = 0.31;
pub const FEAR_INDEX_MAX_GLOBAL: f32 = 0.68;

/// Snapshot of KER-related state and safety envelopes.
/// Mirrors KerSnapshot in ecosystem-ker-profile.v1.aln.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct KerSnapshot {
    pub carbon_removal: f32,
    pub water_restoration: f32,
    pub biodiversity_gain: f32,
    pub toxicity_reduction: f32,
    pub socio_ecolabour: f32,
    pub rohscalar: f32,
    pub lifeforcescalar: f32,
    pub biocompatibilityrating: f32,
    pub painindex: f32,
    pub fearindex: f32,
}

/// Before/after pair for Lyapunov guard evaluation.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct KerDelta {
    pub before: KerSnapshot,
    pub after: KerSnapshot,
}

/// Lyapunov residual V(after) - V(before).
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LyapunovResidual {
    pub v_before: f32,
    pub v_after: f32,
    pub residual: f32,
}

/// Guard decision outcomes (must align with ALN KerGuardDecision).
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum KerGuardDecision {
    Accept,
    RejectSafety,
    RejectNonMonotone,
    RejectDataLaborMissing,
}

/// Minimal description of eco-labour evidence for the window.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EcoLaborEvidenceSummary {
    /// At least one ID must be present for positive KER deltas.
    pub evidence_ids: Vec<String>,
    /// True if at least one evidence row is measurement-tethered.
    pub has_measurement_tethered: bool,
}

/// Compute a KerSnapshot from raw inputs (telemetry, eco-labour metrics).
///
/// This is the canonical entrypoint for AI/CI callers that need a
/// safety-checked KER snapshot before feeding into Lyapunov logic.
/// It should:
/// - Clamp values into ALN-specified domains.
/// - Populate safety envelope scalars from the current host envelopes.
pub fn compute_ker_snapshot(
    carbon_removal: f32,
    water_restoration: f32,
    biodiversity_gain: f32,
    toxicity_reduction: f32,
    socio_ecolabour: f32,
    rohscalar: f32,
    lifeforcescalar: f32,
    biocompatibilityrating: f32,
    painindex: f32,
    fearindex: f32,
) -> KerSnapshot {
    // Implementation should clamp/scalar-normalize according to ALN,
    // but the signature and struct layout are canonical.
    KerSnapshot {
        carbon_removal,
        water_restoration,
        biodiversity_gain,
        toxicity_reduction,
        socio_ecolabour,
        rohscalar,
        lifeforcescalar,
        biocompatibilityrating,
        painindex,
        fearindex,
    }
}

/// Compute the Lyapunov potential V_eco for a given KER snapshot.
///
/// Invariants:
/// - Lower V_eco corresponds to better eco state under fixed safety envelopes.
/// - This function must not widen any corridor or relax safety envelopes.
pub fn compute_v_eco(snapshot: &KerSnapshot) -> f32 {
    // Exact formula is project-specific and should be filled in using
    // your chosen Lyapunov construction; this signature is canonical.
    // Placeholder: return 0.0 to keep the function total; replace with
    // real math in your repo.
    0.0
}

/// Evaluate the KER Lyapunov guard for a before/after pair and eco-labour evidence.
///
/// Returns a KerGuardDecision and the LyapunovResidual.
/// Invariants (to be proven by Kani):
/// - If residual > 0.0, decision MUST be RejectNonMonotone.
/// - If any safety envelope is violated in `after`, decision MUST be RejectSafety.
/// - If KER improves (e.g., V_eco decreases) without eco-labour evidence,
///   decision MUST be RejectDataLaborMissing.
pub fn evaluate_ker_guard(
    delta: &KerDelta,
    eco_evidence: &EcoLaborEvidenceSummary,
) -> (KerGuardDecision, LyapunovResidual) {
    let v_before = compute_v_eco(&delta.before);
    let v_after = compute_v_eco(&delta.after);
    let residual = v_after - v_before;

    let safety_ok = safety_envelopes_ok(&delta.after);
    let has_data_labor = eco_evidence.has_measurement_tethered;

    let decision = if !safety_ok {
        KerGuardDecision::RejectSafety
    } else if residual > 0.0 {
        KerGuardDecision::RejectNonMonotone
    } else if residual < 0.0 && !has_data_labor {
        KerGuardDecision::RejectDataLaborMissing
    } else {
        KerGuardDecision::Accept
    };

    (
        decision,
        LyapunovResidual {
            v_before,
            v_after,
            residual,
        },
    )
}

/// Check that RoH, Lifeforce, BCR, pain, and fear indices obey global envelopes.
pub fn safety_envelopes_ok(snapshot: &KerSnapshot) -> bool {
    snapshot.rohscalar <= ROH_CEILING_GLOBAL
        && snapshot.liforcescalar >= LIFORCEFLOOR_GLOBAL
        && snapshot.biocompatibilityrating >= BCR_MIN_GLOBAL
        && snapshot.painindex <= PAIN_INDEX_CEILING_GLOBAL
        && snapshot.fearindex >= FEAR_INDEX_MIN_GLOBAL
        && snapshot.fearindex <= FEAR_INDEX_MAX_GLOBAL
}
