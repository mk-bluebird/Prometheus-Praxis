//! Configuration for ecosafety diagnostics.
//!
//! `EcosafetyConfig` encodes corridor thresholds, covariance quality
//! limits, and minimal sample counts for ecosafety envelopes.
//!
//! These defaults are intended to align with the Phoenix ecosafety
//! envelope policy `CyboquaticEcosafetyEnvelopePhoenix2026v1` and its
//! SQL migration, while remaining overridable per‑region or per‑node.

/// Configuration for ecosafety thresholds and quality checks.
///
/// All thresholds are expressed in the same normalised units used by
/// the risk coordinates in `NodeRiskSample` and the ecosafety envelope
/// fields in `CyboNodeEcosafetyEnvelope`.
#[derive(Clone, Debug)]
pub struct EcosafetyConfig {
    /// Advisory ecosafety distance threshold.
    ///
    /// Distances `d` with `d <= d_warn_default` are classified as
    /// `GREEN` when covariance quality is acceptable.
    pub d_warn_default: f32,

    /// Hard ecosafety distance threshold.
    ///
    /// Distances `d` with `d > d_max_default` are classified as `RED`
    /// when covariance quality is acceptable.
    pub d_max_default: f32,

    /// Maximum acceptable covariance condition number.
    ///
    /// Windows with `cond > max_cov_condition` are treated as
    /// `UNDEFINED` due to degenerate covariance.
    pub max_cov_condition: f32,

    /// Minimum number of samples required in a window.
    ///
    /// Windows with `samples_used < min_samples` are treated as
    /// `UNDEFINED` regardless of distance.
    pub min_samples: usize,

    /// Knowledge factor K for this ecosafety module.
    ///
    /// This can be set from the `knowledge_ecoscore` table; a typical
    /// Phoenix value is approximately 0.94.
    pub kfactor: f32,

    /// Eco‑impact factor E for this ecosafety module.
    ///
    /// This can be set from the `knowledge_ecoscore` table; a typical
    /// Phoenix value is approximately 0.90.
    pub efactor: f32,

    /// Risk‑of‑harm factor R for this ecosafety module.
    ///
    /// This can be set from the `knowledge_ecoscore` table; a typical
    /// Phoenix value is approximately 0.13.
    pub rfactor: f32,
}

impl Default for EcosafetyConfig {
    fn default() -> Self {
        Self {
            d_warn_default: 2.0,
            d_max_default: 3.0,
            max_cov_condition: 1_000.0,
            min_samples: 5,
            kfactor: 0.94,
            efactor: 0.90,
            rfactor: 0.13,
        }
    }
}
