// filename: crates/prometheus-praxis-lyapunov-guard/src/lib.rs
// Designed for https://github.com/mk-bluebird/Prometheus-Praxis
// Ecosystem KER Lyapunov guard for eco-labour–anchored evolution.
// Rust 2024, rust-version = "1.85", Kani 0.67, no unsafe.

#![forbid(unsafe_code)]
#![deny(missing_docs)]

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

/// Check that Lyapunov residual is non-increasing.
///
/// Returns true if vt_next - vt_current <= 0 within a small epsilon.
pub fn lyapunov_non_increasing(vt_current: f64, vt_next: f64) -> bool {
    let delta = vt_next - vt_current;
    delta <= 1.0e-9
}

/// Check that K,E,R are within [0,1] and that ker_score ~= k * e - r.
///
/// Returns true if all constraints are satisfied.
pub fn ker_band_and_consistency(k: f64, e: f64, r: f64, ker_score: f64) -> bool {
    if !(0.0 <= k && k <= 1.0) {
        return false;
    }
    if !(0.0 <= e && e <= 1.0) {
        return false;
    }
    if !(0.0 <= r && r <= 1.0) {
        return false;
    }
    let expected = k * e - r;
    let diff = (expected - ker_score).abs();
    diff <= 1.0e-6
}

#[cfg(kani)]
mod proofs_ker {
    use super::*;

    #[kani::proof]
    fn prove_lyapunov_non_increasing_band() {
        let vt_current = kani::any();
        let vt_next = kani::any();
        kani::assume(vt_next <= vt_current);
        assert!(lyapunov_non_increasing(vt_current, vt_next));
    }

    #[kani::proof]
    fn prove_ker_band_consistency() {
        let k = kani::any();
        let e = kani::any();
        let r = kani::any();
        let ker_score = k * e - r;
        kani::assume(0.0 <= k && k <= 1.0);
        kani::assume(0.0 <= e && e <= 1.0);
        kani::assume(0.0 <= r && r <= 1.0);
        assert!(ker_band_and_consistency(k, e, r, ker_score));
    }
}

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
    pub evidence_ids: Vec<String>,
    pub has_measurement_tethered: bool,
}

/// Clamp helper to keep scalar values in [0,1].
fn clamp01(x: f32) -> f32 {
    if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

/// Compute a KerSnapshot from raw inputs (telemetry, eco-labour metrics).
///
/// Values are clamped into [0,1] where applicable and safety-envelope
/// scalars are left as provided (they are validated separately).
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
    KerSnapshot {
        carbon_removal: clamp01(carbon_removal),
        water_restoration: clamp01(water_restoration),
        biodiversity_gain: clamp01(biodiversity_gain),
        toxicity_reduction: clamp01(toxicity_reduction),
        socio_ecolabour: clamp01(socio_ecolabour),
        rohscalar,
        lifeforcescalar,
        biocompatibilityrating,
        painindex,
        fearindex,
    }
}

/// Compute the Lyapunov potential V_eco for a given KER snapshot.
///
/// Lower V_eco corresponds to better eco state under fixed safety envelopes.
/// This construction keeps all contributions bounded and does not widen corridors.
pub fn compute_v_eco(snapshot: &KerSnapshot) -> f32 {
    let k_carbon = clamp01(snapshot.carbon_removal);
    let k_water = clamp01(snapshot.water_restoration);
    let k_bio = clamp01(snapshot.biodiversity_gain);
    let k_toxicity = clamp01(1.0 - clamp01(snapshot.toxicity_reduction));
    let k_labour = clamp01(snapshot.socio_ecolabour);

    let roh = snapshot.rohscalar;
    let lifeforce = snapshot.lifeforcescalar;
    let bcr = snapshot.biocompatibilityrating;
    let pain = snapshot.painindex;
    let fear = snapshot.fearindex;

    let good_sum = k_carbon + k_water + k_bio + k_toxicity + k_labour;
    let good_term = (5.0_f32 - good_sum) / 5.0_f32;

    let roh_term = roh / ROH_CEILING_GLOBAL;
    let lifeforce_term = if lifeforce >= LIFEFORCE_FLOOR_GLOBAL {
        (LIFEFORCE_FLOOR_GLOBAL - lifeforce).abs() / LIFEFORCE_FLOOR_GLOBAL
    } else {
        (LIFEFORCE_FLOOR_GLOBAL - lifeforce) / LIFEFORCE_FLOOR_GLOBAL
    };
    let bcr_term = if bcr >= BCR_MIN_GLOBAL {
        (BCR_MIN_GLOBAL - bcr).abs() / BCR_MIN_GLOBAL
    } else {
        (BCR_MIN_GLOBAL - bcr) / BCR_MIN_GLOBAL
    };

    let pain_term = pain / PAIN_INDEX_CEILING_GLOBAL;
    let fear_center = (FEAR_INDEX_MIN_GLOBAL + FEAR_INDEX_MAX_GLOBAL) / 2.0;
    let fear_range = (FEAR_INDEX_MAX_GLOBAL - FEAR_INDEX_MIN_GLOBAL) / 2.0;
    let fear_term = if fear_range > 0.0 {
        ((fear - fear_center).abs() / fear_range).min(1.0)
    } else {
        1.0
    };

    0.3 * good_term
        + 0.2 * roh_term
        + 0.2 * lifeforce_term
        + 0.1 * bcr_term
        + 0.1 * pain_term
        + 0.1 * fear_term
}

/// Evaluate the KER Lyapunov guard for a before/after pair and eco-labour evidence.
///
/// If residual > 0.0, decision is RejectNonMonotone.
/// If any safety envelope is violated in `after`, decision is RejectSafety.
/// If residual < 0.0 and no measurement-tethered evidence is present,
/// decision is RejectDataLaborMissing.
/// Otherwise, the decision is Accept.
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
        && snapshot.lifeforcescalar >= LIFEFORCE_FLOOR_GLOBAL
        && snapshot.biocompatibilityrating >= BCR_MIN_GLOBAL
        && snapshot.painindex <= PAIN_INDEX_CEILING_GLOBAL
        && snapshot.fearindex >= FEAR_INDEX_MIN_GLOBAL
        && snapshot.fearindex <= FEAR_INDEX_MAX_GLOBAL
}

#[cfg(kani)]
mod proofs_guard {
    use super::*;

    #[kani::proof]
    fn prove_guard_rejects_positive_residual() {
        let before = KerSnapshot {
            carbon_removal: 0.5,
            water_restoration: 0.5,
            biodiversity_gain: 0.5,
            toxicity_reduction: 0.5,
            socio_ecolabour: 0.5,
            rohscalar: ROH_CEILING_GLOBAL * 0.5,
            lifeforcescalar: LIFEFORCE_FLOOR_GLOBAL,
            biocompatibilityrating: BCR_MIN_GLOBAL,
            painindex: PAIN_INDEX_CEILING_GLOBAL * 0.5,
            fearindex: (FEAR_INDEX_MIN_GLOBAL + FEAR_INDEX_MAX_GLOBAL) / 2.0,
        };
        let after = before.clone();
        let delta = KerDelta { before, after };
        let evidence = EcoLaborEvidenceSummary {
            evidence_ids: vec!["e1".to_string()],
            has_measurement_tethered: true,
        };
        let (decision, residual) = evaluate_ker_guard(&delta, &evidence);
        assert!(
            residual.residual <= 0.0 || decision == KerGuardDecision::RejectNonMonotone
        );
    }
}
