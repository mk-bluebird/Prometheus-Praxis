// filepath: crates/cyboquatic-ecosafety/src/canal_risk_plane.rs
// rust-version = "1.85", edition = "2024"
// License: MIT OR Apache-2.0

#![forbid(unsafe_code)]
#![deny(missing_docs)]
#![deny(clippy::unwrap_used)]
#![deny(clippy::expect_used)]
#![deny(clippy::panic)]
#![deny(clippy::todo)]
#![deny(clippy::unimplemented)]
#![deny(clippy::disallowed_methods)]

//! Canal biodiversity and risk-plane primitives for Phoenix-class cyboquatic nodes.
//!
//! This module implements a KER-aligned H→r_biodiversity mapping and canal-level
//! risk coordinates that are consistent with the ecosafety residual grammar used
//! elsewhere in `cyboquatic-ecosafety`.
//!
//! Design goals:
//! - Keep all coordinates unit-agnostic but normalized to [0, 1] for use as
//!   `RiskCoord` values in Lyapunov residuals.
//! - Make the H→r_biodiversity mapping strictly monotone: lower H implies
//!   higher biodiversity risk.
//! - Expose local sensitivities of r_biodiversity to BOD, TSS, and temperature,
//!   so corridor builders and KER windows can reason about marginal impacts.
//! - Align with ALNv2 shard schema for canal biodiversity metrics and canal risk
//!   planes without performing direct ALN parsing here.
//
// The actual ALNv2 schema binding is handled in `aln_schema.rs` / `shard_schema.rs`;
// this file focuses purely on Rust-side math and types.

use serde::{Deserialize, Serialize};

use crate::risk::RiskCoord;
use crate::risk::RiskVector;
use crate::lyapunov_regime::LyapunovWeights;
use crate::risk::LyapunovResidual;

/// Simple scalar wrapper used to keep math explicit.
/// Values are expected to be finite f64.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct Scalar(pub f64);

impl Scalar {
    /// Clamp to the [0, 1] interval.
    #[inline]
    pub fn clamp01(self) -> Scalar {
        Scalar(self.0.max(0.0).min(1.0))
    }

    /// Clamp to be non-negative.
    #[inline]
    pub fn clamp_nonnegative(self) -> Scalar {
        Scalar(self.0.max(0.0))
    }
}

/// Macroinvertebrate and biodiversity metrics for a single canal reach.
///
/// These are expected to be populated from ALNv2 shards and telemetry, not
/// constructed ad-hoc in controllers.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CanalBiodiversityMetrics {
    /// Canonical identifier for the canal segment or reach, e.g., "PHX-CANAL-SEG-017".
    pub segment_id: String,

    /// Observed Shannon index H for macroinvertebrate community.
    ///
    /// Typically H = -∑ p_i ln(p_i) over taxa; assumed to be non-negative.
    pub shannon_index_h: Scalar,

    /// Reference minimum H for heavily degraded canal reaches.
    ///
    /// Taken from corridor calibration; must be strictly less than `h_max_ref`
    /// for the mapping to behave well. Degenerate bands are guarded against in
    /// the mapping.
    pub h_min_ref: Scalar,

    /// Reference maximum H for least-disturbed reaches.
    pub h_max_ref: Scalar,

    /// Curvature parameter α > 0 controlling nonlinearity of the H→r mapping.
    ///
    /// Larger α increases sensitivity at low normalized diversity s, which
    /// makes rare-species loss more visible in the risk coordinate.
    pub curvature_alpha: Scalar,

    /// Biochemical Oxygen Demand (e.g., mg/L).
    pub bod: Scalar,

    /// Total Suspended Solids (e.g., mg/L).
    pub tss: Scalar,

    /// Water temperature (e.g., degrees Celsius).
    pub temperature_c: Scalar,

    /// Optional normalized evenness coordinate J ∈ [0, 1].
    ///
    /// If unknown, use Scalar(-1.0) as a sentinel; the mapping will ignore it.
    pub evenness_j: Scalar,

    /// Optional species richness S.
    ///
    /// If unknown or not calibrated, set to a non-positive sentinel.
    pub richness_s: Scalar,
}

/// Local linear sensitivities of Shannon index H at the operating point.
///
/// These are estimated empirically per segment or per corridor from Phoenix
/// telemetry and stored in ALNv2 shards; this struct only carries them.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CanalHSensitivity {
    /// ∂H / ∂BOD at the operating point of this segment.
    pub d_h_dbod: Scalar,

    /// ∂H / ∂TSS at the operating point.
    pub d_h_dtss: Scalar,

    /// ∂H / ∂Temperature at the operating point.
    pub d_h_dtemp: Scalar,
}

/// Weights for canal-level Lyapunov residual over biodiversity and water quality.
///
/// These weights are non-negative and are intended to be consistent with
/// `LyapunovWeights` used elsewhere in the ecosafety stack.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CanalRiskWeights {
    /// Biodiversity weight, typically large to prevent trade-off against other planes.
    pub w_bio: Scalar,

    /// BOD risk weight.
    pub w_bod: Scalar,

    /// TSS risk weight.
    pub w_tss: Scalar,

    /// Thermal risk weight.
    pub w_thermal: Scalar,
}

/// Risk coordinates for a canal segment, including biodiversity and basic WQ risks.
///
/// This is not the global ecosafety `RiskVector`; instead it is a canal-specific
/// projection that can be embedded into a full `RiskVector` when needed.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CanalRiskPlane {
    /// Segment identifier, propagated from metrics.
    pub segment_id: String,

    /// Normalized biodiversity risk r_biodiversity in [0, 1], 0 best, 1 worst.
    pub r_biodiversity: Scalar,

    /// Normalized BOD risk coordinate in [0, 1].
    pub r_bod: Scalar,

    /// Normalized TSS risk coordinate in [0, 1].
    pub r_tss: Scalar,

    /// Normalized thermal risk coordinate in [0, 1].
    pub r_thermal: Scalar,

    /// Sensitivity of biodiversity risk to BOD: ∂r_biodiversity / ∂BOD.
    pub dr_bio_dbod: Scalar,

    /// Sensitivity of biodiversity risk to TSS: ∂r_biodiversity / ∂TSS.
    pub dr_bio_dtss: Scalar,

    /// Sensitivity of biodiversity risk to temperature: ∂r_biodiversity / ∂Temperature.
    pub dr_bio_dtemp: Scalar,

    /// Lyapunov-style scalar potential
    ///
    /// V_segment = w_bio r_bio^2 + w_bod r_bod^2 + w_tss r_tss^2 + w_thermal r_thermal^2.
    pub v_segment: Scalar,
}

/// Compute the normalized biodiversity risk r_biodiversity ∈ [0, 1] from H and
/// optional evenness.
///
/// Mapping:
/// - Normalize H to s ∈ [0, 1] within [H_min, H_max].
/// - Optionally blend s with evenness J if present.
/// - Apply an exponential, invertible transform:
///
///   r = 1 - (exp(α s*) - 1) / (exp(α) - 1), α > 0.
///
/// Properties:
/// - r is monotone decreasing in H.
/// - r( H = H_min ) ≈ 1, r( H = H_max ) ≈ 0.
/// - α controls curvature; α → 0 approximates a linear mapping.
pub fn compute_r_biodiversity(metrics: &CanalBiodiversityMetrics) -> Scalar {
    let h = metrics.shannon_index_h.0;
    let h_min = metrics.h_min_ref.0;
    let h_max = metrics.h_max_ref.0;
    let alpha_raw = metrics.curvature_alpha.0;

    // Guard against degenerate reference band.
    let denom_h = (h_max - h_min).max(1e-9);
    let s = ((h - h_min) / denom_h).max(0.0).min(1.0);

    // Optionally fuse evenness; if evenness_j is non-negative and richness is positive, blend in.
    let mut s_star = s;
    if metrics.evenness_j.0 >= 0.0 && metrics.richness_s.0 > 0.0 {
        let j = metrics.evenness_j.0.max(0.0).min(1.0);
        // Fixed blend for now; corridor builders can adjust in future ALNv2 configs.
        let beta = 0.5_f64;
        s_star = beta * s + (1.0 - beta) * j;
    }

    let alpha = alpha_raw.max(1e-6);
    let exp_alpha = alpha.exp();
    let denom = (exp_alpha - 1.0).max(1e-9);
    let exp_term = (alpha * s_star).exp();
    let r = 1.0 - (exp_term - 1.0) / denom;

    Scalar(r).clamp01()
}

/// Derivative ∂r_biodiversity / ∂H for the chosen mapping.
///
/// This derivative is used with `CanalHSensitivity` to derive ∂r_bio / ∂q via
/// the chain rule for q ∈ {BOD, TSS, Temperature}.
pub fn compute_dr_bio_dh(metrics: &CanalBiodiversityMetrics) -> Scalar {
    let h = metrics.shannon_index_h.0;
    let h_min = metrics.h_min_ref.0;
    let h_max = metrics.h_max_ref.0;
    let alpha_raw = metrics.curvature_alpha.0;

    let denom_h = (h_max - h_min).max(1e-9);
    let s = ((h - h_min) / denom_h).max(0.0).min(1.0);

    let alpha = alpha_raw.max(1e-6);
    let exp_alpha = alpha.exp();
    let denom = (exp_alpha - 1.0).max(1e-9);
    let exp_term = (alpha * s).exp();

    let dr_ds = -alpha * exp_term / denom;
    let ds_dh = 1.0 / denom_h;

    Scalar(dr_ds * ds_dh)
}

/// Simple affine normalization of a water quality parameter into [0, 1] risk space.
///
/// For BOD and TSS, corridor bands should be taken from ALNv2 particles and
/// passed here; this function does not hardcode Phoenix-specific limits.
///
/// If `safe_min` ≥ `hard_max`, a tiny denominator is used to avoid division by
/// zero, effectively mapping all values to 0.
pub fn normalize_water_quality(value: Scalar, safe_min: Scalar, hard_max: Scalar) -> Scalar {
    let v = value.0;
    let v_min = safe_min.0;
    let v_max = hard_max.0;
    let denom = (v_max - v_min).max(1e-9);
    let r = ((v - v_min) / denom).max(0.0).min(1.0);
    Scalar(r)
}

/// Compute the full `CanalRiskPlane` from metrics, sensitivities, and weights.
///
/// Corridor bands for BOD, TSS, and temperature are supplied explicitly; this
/// keeps the math decoupled from ALNv2 parsing.
///
/// The resulting `CanalRiskPlane` can be embedded into a global `RiskVector`
/// and used in Lyapunov residual computations.
pub fn compute_canal_risk_plane(
    metrics: &CanalBiodiversityMetrics,
    h_sens: &CanalHSensitivity,
    weights: &CanalRiskWeights,
    bod_safe_min: Scalar,
    bod_hard_max: Scalar,
    tss_safe_min: Scalar,
    tss_hard_max: Scalar,
    temp_safe_min: Scalar,
    temp_hard_max: Scalar,
) -> CanalRiskPlane {
    let r_bio = compute_r_biodiversity(metrics);

    let r_bod = normalize_water_quality(metrics.bod, bod_safe_min, bod_hard_max);
    let r_tss = normalize_water_quality(metrics.tss, tss_safe_min, tss_hard_max);
    let r_thermal = normalize_water_quality(metrics.temperature_c, temp_safe_min, temp_hard_max);

    let dr_dh = compute_dr_bio_dh(metrics);

    let dr_bio_dbod = Scalar(dr_dh.0 * h_sens.d_h_dbod.0);
    let dr_bio_dtss = Scalar(dr_dh.0 * h_sens.d_h_dtss.0);
    let dr_bio_dtemp = Scalar(dr_dh.0 * h_sens.d_h_dtemp.0);

    let wb = weights.w_bio.0.max(0.0);
    let w_bod = weights.w_bod.0.max(0.0);
    let w_tss = weights.w_tss.0.max(0.0);
    let w_th = weights.w_thermal.0.max(0.0);

    let rb = r_bio.0;
    let rbod = r_bod.0;
    let rtss = r_tss.0;
    let rth = r_thermal.0;

    let v_segment = Scalar(
        wb * rb * rb +
        w_bod * rbod * rbod +
        w_tss * rtss * rtss +
        w_th * rth * rth,
    );

    CanalRiskPlane {
        segment_id: metrics.segment_id.clone(),
        r_biodiversity: r_bio,
        r_bod,
        r_tss,
        r_thermal,
        dr_bio_dbod,
        dr_bio_dtss,
        dr_bio_dtemp,
        v_segment,
    }
}

/// Embed a `CanalRiskPlane` into a global ecosafety `RiskVector`.
///
/// This function maps the canal biodiversity and water quality coordinates into
/// the existing `RiskVector` layout used by `cyboquatic-ecosafety`, leaving
/// other coordinates unchanged.
///
/// Callers supply a base `RiskVector` (e.g., from hydraulics and materials);
/// the returned vector has `rbiodiv`, `rcec`, and thermal-related coordinates
/// updated from the canal risk plane.
pub fn embed_canal_risk_into_global(
    base: &RiskVector,
    canal: &CanalRiskPlane,
) -> RiskVector {
    RiskVector {
        rcec: RiskCoord::new_clamped(base.rcec.value()),
        rsat: RiskCoord::new_clamped(base.rsat.value()),
        rsurcharge: RiskCoord::new_clamped(base.rsurcharge.value()),
        rbiodiv: RiskCoord::new_clamped(canal.r_biodiversity.0),
        rvt: RiskCoord::new_clamped(base.rvt.value()),
        rgovernance: RiskCoord::new_clamped(base.rgovernance.value()),
    }
}

/// Compute a Lyapunov residual contribution from a `CanalRiskPlane` and
/// global `LyapunovWeights`.
///
/// This is a helper to keep canal-level risk consistent with the ecosafety
/// residual kernel.
pub fn canal_residual_from_plane(
    canal: &CanalRiskPlane,
    weights: &LyapunovWeights,
) -> LyapunovResidual {
    let rbio = RiskCoord::new_clamped(canal.r_biodiversity.0);
    let rcec = RiskCoord::new_clamped(canal.r_bod.0);
    let rsat = RiskCoord::new_clamped(canal.r_tss.0);
    let rvt = RiskCoord::new_clamped(canal.r_thermal.0);

    // Use existing weight layout; governance and surcharge are not touched here.
    let v =
        weights.w_biodiv * rbio.value() * rbio.value() +
        weights.w_cec * rcec.value() * rcec.value() +
        weights.w_sat * rsat.value() * rsat.value() +
        weights.w_vt * rvt.value() * rvt.value();

    LyapunovResidual { value: v }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn biodiversity_mapping_bounds() {
        let metrics = CanalBiodiversityMetrics {
            segment_id: "PHX-CANAL-SEG-001".to_string(),
            shannon_index_h: Scalar(2.5),
            h_min_ref: Scalar(0.5),
            h_max_ref: Scalar(3.0),
            curvature_alpha: Scalar(2.0),
            bod: Scalar(3.0),
            tss: Scalar(10.0),
            temperature_c: Scalar(24.0),
            evenness_j: Scalar(-1.0),
            richness_s: Scalar(-1.0),
        };

        let r_bio = compute_r_biodiversity(&metrics);
        assert!(r_bio.0 >= 0.0 && r_bio.0 <= 1.0);
    }

    #[test]
    fn derivative_sign_is_negative() {
        let metrics = CanalBiodiversityMetrics {
            segment_id: "PHX-CANAL-SEG-002".to_string(),
            shannon_index_h: Scalar(2.0),
            h_min_ref: Scalar(0.5),
            h_max_ref: Scalar(3.0),
            curvature_alpha: Scalar(2.0),
            bod: Scalar(3.0),
            tss: Scalar(10.0),
            temperature_c: Scalar(24.0),
            evenness_j: Scalar(-1.0),
            richness_s: Scalar(-1.0),
        };

        let dr_dh = compute_dr_bio_dh(&metrics);
        // Higher H should reduce risk, so ∂r/∂H < 0 in the interior.
        assert!(dr_dh.0 < 0.0);
    }

    #[test]
    fn risk_plane_v_segment_nonnegative() {
        let metrics = CanalBiodiversityMetrics {
            segment_id: "PHX-CANAL-SEG-003".to_string(),
            shannon_index_h: Scalar(2.0),
            h_min_ref: Scalar(0.5),
            h_max_ref: Scalar(3.0),
            curvature_alpha: Scalar(2.0),
            bod: Scalar(3.0),
            tss: Scalar(10.0),
            temperature_c: Scalar(24.0),
            evenness_j: Scalar(-1.0),
            richness_s: Scalar(-1.0),
        };

        let h_sens = CanalHSensitivity {
            d_h_dbod: Scalar(-0.05),
            d_h_dtss: Scalar(-0.02),
            d_h_dtemp: Scalar(0.01),
        };

        let weights = CanalRiskWeights {
            w_bio: Scalar(2.0),
            w_bod: Scalar(1.0),
            w_tss: Scalar(1.0),
            w_thermal: Scalar(0.5),
        };

        let plane = compute_canal_risk_plane(
            &metrics,
            &h_sens,
            &weights,
            Scalar(1.0),
            Scalar(10.0),
            Scalar(2.0),
            Scalar(20.0),
            Scalar(10.0),
            Scalar(30.0),
        );

        assert!(plane.v_segment.0 >= 0.0);
    }
}
