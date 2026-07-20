// rust-version = "1.85", edition = "2024"
// Kani verifier is required elsewhere in the workspace; this file is written to be Kani-compatible.
// No unsafe code is used; all math is explicit and unit-agnostic.

use serde::{Deserialize, Serialize};

/// Scalar wrapper used throughout EcoNet-style crates.
/// Values are expected to be in physically meaningful units, but the math here is unit-agnostic.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct Scalar(pub f64);

impl Scalar {
    #[inline]
    pub fn clamp01(self) -> Scalar {
        Scalar(self.0.max(0.0).min(1.0))
    }

    #[inline]
    pub fn clamp_nonnegative(self) -> Scalar {
        Scalar(self.0.max(0.0))
    }
}

/// Basic macroinvertebrate community metrics for a single Phoenix canal reach.
/// This is the raw ecological substrate from which Shannon index and risk coordinates are derived.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CanalBiodiversityMetrics {
    /// Canonical identifier for the canal segment or reach, e.g., "PHX-CANAL-SEG-017".
    pub segment_id: String,
    /// Observed Shannon index H for macroinvertebrate community.
    /// Typically computed as H = -sum_i p_i ln(p_i) over taxa.
    pub shannon_index_h: Scalar,
    /// Reference minimum H for heavily degraded canal reaches.
    /// This is set from corridor data and telemetry, not hardcoded.
    pub h_min_ref: Scalar,
    /// Reference maximum H for least-disturbed reaches.
    pub h_max_ref: Scalar,
    /// Curvature parameter alpha > 0 controlling nonlinearity of the H -> r_biodiversity mapping.
    /// Larger alpha increases sensitivity to losses in rare species at low diversity.
    pub curvature_alpha: Scalar,
    /// Biochemical Oxygen Demand (e.g., mg/L).
    pub bod: Scalar,
    /// Total Suspended Solids (e.g., mg/L).
    pub tss: Scalar,
    /// Water temperature (e.g., degrees Celsius).
    pub temperature_c: Scalar,
    /// Optional normalized evenness coordinate J = H / ln(S), where S is species richness.
    /// If not available, use Scalar(-1.0) as a sentinel.
    pub evenness_j: Scalar,
    /// Optional species richness S. If unknown, set to Scalar(-1.0) or 0.0 depending on corridor conventions.
    pub richness_s: Scalar,
}

/// Risk coordinates for a canal segment, including biodiversity and basic water quality risks.
/// These conform to the EcoFort / EcoNet pattern of normalized scalars in [0,1] feeding Lyapunov residuals.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CanalRiskPlane {
    /// Segment identifier, propagated from metrics.
    pub segment_id: String,
    /// Normalized biodiversity risk r_biodiversity in [0,1], 0 best, 1 worst.
    pub r_biodiversity: Scalar,
    /// Normalized BOD risk coordinate in [0,1].
    pub r_bod: Scalar,
    /// Normalized TSS risk coordinate in [0,1].
    pub r_tss: Scalar,
    /// Normalized thermal risk coordinate in [0,1].
    pub r_thermal: Scalar,
    /// Sensitivity of biodiversity risk to BOD: ∂r_biodiversity / ∂BOD.
    pub dr_bio_dbod: Scalar,
    /// Sensitivity of biodiversity risk to TSS: ∂r_biodiversity / ∂TSS.
    pub dr_bio_dtss: Scalar,
    /// Sensitivity of biodiversity risk to temperature: ∂r_biodiversity / ∂Temperature.
    pub dr_bio_dtemp: Scalar,
    /// Lyapunov-style scalar potential V_segment = w_bio r_bio^2 + w_bod r_bod^2 + w_tss r_tss^2 + w_thermal r_thermal^2.
    pub v_segment: Scalar,
}

/// Weight configuration for the canal risk plane. All weights are non-negative.
/// Biodiversity weight should generally be high to prevent trade-off against other coordinates.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CanalRiskWeights {
    pub w_bio: Scalar,
    pub w_bod: Scalar,
    pub w_tss: Scalar,
    pub w_thermal: Scalar,
}

/// Local linear sensitivities of Shannon index H with respect to water quality parameters.
/// These are estimated empirically per segment or per corridor from Phoenix telemetry
/// and plugged here to derive ∂r_bio / ∂q via the chain rule.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CanalHSensitivity {
    /// ∂H / ∂BOD at the operating point of this segment.
    pub d_h_dbod: Scalar,
    /// ∂H / ∂TSS at the operating point.
    pub d_h_dtss: Scalar,
    /// ∂H / ∂Temperature at the operating point.
    pub d_h_dtemp: Scalar,
}

/// Core mapping from Shannon index and optional evenness to normalized biodiversity risk r_biodiversity.
/// This implements the exponential, invertible transform described in the EcoNet corridor notes.
pub fn compute_r_biodiversity(metrics: &CanalBiodiversityMetrics) -> Scalar {
    let h = metrics.shannon_index_h.0;
    let h_min = metrics.h_min_ref.0;
    let h_max = metrics.h_max_ref.0;
    let alpha = metrics.curvature_alpha.0;

    // Guard against degenerate reference band.
    let denom_h = (h_max - h_min).max(1e-9);
    let s = ((h - h_min) / denom_h).max(0.0).min(1.0);

    // Optionally fuse evenness; if evenness_j is non-negative and richness is positive, blend in.
    let mut s_star = s;
    if metrics.evenness_j.0 >= 0.0 && metrics.richness_s.0 > 0.0 {
        // Pielou's evenness J = H / ln(S); here evenness_j is assumed already computed and normalized to [0,1].
        let j = metrics.evenness_j.0.max(0.0).min(1.0);
        // Blend richness-normalized diversity s with evenness j.
        let beta = 0.5_f64; // richness vs evenness weighting; could be corridor-configurable.
        s_star = beta * s + (1.0 - beta) * j;
    }

    // Exponential, invertible mapping from s* in [0,1] to r in [0,1].
    // r = 1 - (exp(alpha s*) - 1) / (exp(alpha) - 1), alpha > 0.
    let alpha_clamped = alpha.max(1e-6);
    let exp_alpha = alpha_clamped.exp();
    let denom = (exp_alpha - 1.0).max(1e-9);
    let exp_term = (alpha_clamped * s_star).exp();
    let r = 1.0 - (exp_term - 1.0) / denom;

    Scalar(r).clamp01()
}

/// Derivative ∂r_biodiversity / ∂H for the chosen mapping.
/// This is used with CanalHSensitivity to derive ∂r_bio / ∂q via chain rule.
pub fn compute_dr_bio_dh(metrics: &CanalBiodiversityMetrics) -> Scalar {
    let h = metrics.shannon_index_h.0;
    let h_min = metrics.h_min_ref.0;
    let h_max = metrics.h_max_ref.0;
    let alpha = metrics.curvature_alpha.0;

    let denom_h = (h_max - h_min).max(1e-9);
    let s = ((h - h_min) / denom_h).max(0.0).min(1.0);

    // As above, we may have fused s and evenness into s*, but for derivative we treat s explicitly.
    let alpha_clamped = alpha.max(1e-6);
    let exp_alpha = alpha_clamped.exp();
    let denom = (exp_alpha - 1.0).max(1e-9);
    let exp_term = (alpha_clamped * s).exp();

    let dr_ds = -alpha_clamped * exp_term / denom;
    let ds_dh = 1.0 / denom_h;

    Scalar(dr_ds * ds_dh)
}

/// Simple normalization of a water quality parameter into [0,1] risk space given corridor bands.
/// For BOD and TSS, corridor bounds should be stored in ALNv2 particles and passed here.
/// This function is unit-agnostic and simply applies affine normalization plus clamping.
pub fn normalize_water_quality(
    value: Scalar,
    safe_min: Scalar,
    hard_max: Scalar,
) -> Scalar {
    let v = value.0;
    let v_min = safe_min.0;
    let v_max = hard_max.0;
    let denom = (v_max - v_min).max(1e-9);
    let r = ((v - v_min) / denom).max(0.0).min(1.0);
    Scalar(r)
}

/// Compute the full CanalRiskPlane from metrics, sensitivities, and weight configuration.
/// Corridor bands for BOD, TSS, and temperature should be provided from ALNv2 via higher layers.
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

    let v_segment = {
        let wb = weights.w_bio.0.max(0.0);
        let w_bod = weights.w_bod.0.max(0.0);
        let w_tss = weights.w_tss.0.max(0.0);
        let w_th = weights.w_thermal.0.max(0.0);

        let rb = r_bio.0;
        let rbod = r_bod.0;
        let rtss = r_tss.0;
        let rth = r_thermal.0;

        Scalar(
            wb * rb * rb +
            w_bod * rbod * rbod +
            w_tss * rtss * rtss +
            w_th * rth * rth,
        )
    };

    CanalRiskPlane {
        segment_id: metrics.segment_id.clone(),
        r_biodiversity: r_bio,
        r_bod: r_bod,
        r_tss: r_tss,
        r_thermal: r_thermal,
        dr_bio_dbod,
        dr_bio_dtss,
        dr_bio_dtemp,
        v_segment,
    }
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
            Scalar(8.0),
            Scalar(5.0),
            Scalar(40.0),
            Scalar(15.0),
            Scalar(35.0),
        );

        assert!(plane.v_segment.0 >= 0.0);
    }
}
