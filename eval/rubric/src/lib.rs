// Path: Prometheus-Praxis/eval/rubric/src/lib.rs
// License: MIT OR Apache-2.0

#![deny(unsafe_code)]
#![forbid(hidden_glob_reexports)]

use serde::{Deserialize, Serialize};

/// Normalized score for each dimension: 0.0 (unacceptable) .. 1.0 (excellent).
/// Threshold checks are explicit to make governance decisions inspectable.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct Score(pub f32);

impl Score {
    pub fn clamp(self) -> Self {
        let Score(v) = self;
        Score(v.max(0.0).min(1.0))
    }

    pub fn is_min(self, min: f32) -> bool {
        let Score(v) = self;
        v >= min
    }
}

/// Seven scoring dimensions used across Prometheus-Praxis.
/// The names follow the evaluation matrix: knowledge, eco, risk, robustness,
/// sovereignty, energy, and governance alignment.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub enum Dimension {
    KnowledgeFactor,
    EcoImpact,
    RiskOfHarm,
    Robustness,
    Sovereignty,
    EnergyEfficiency,
    GovernanceAlignment,
}

/// A full seven-dimension profile for a single module or system.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SevenDimProfile {
    pub knowledge_factor: Score,
    pub eco_impact: Score,
    pub risk_of_harm: Score,
    pub robustness: Score,
    pub sovereignty: Score,
    pub energy_efficiency: Score,
    pub governance_alignment: Score,
}

impl SevenDimProfile {
    /// Minimum score across all seven dimensions.
    /// Useful for quick sanity checks and coarse ranking.
    pub fn min_score(&self) -> Score {
        let scores = [
            self.knowledge_factor.0,
            self.eco_impact.0,
            self.risk_of_harm.0,
            self.robustness.0,
            self.sovereignty.0,
            self.energy_efficiency.0,
            self.governance_alignment.0,
        ];
        let min = scores
            .iter()
            .fold(1.0_f32, |acc, v| if *v < acc { *v } else { acc });
        Score(min)
    }

    /// Check that all dimensions meet a minimum threshold.
    /// Used for both component-level and system-level gates.
    pub fn satisfies_threshold(&self, per_dim_min: f32) -> bool {
        self.knowledge_factor.is_min(per_dim_min)
            && self.eco_impact.is_min(per_dim_min)
            && self.risk_of_harm.is_min(per_dim_min)
            && self.robustness.is_min(per_dim_min)
            && self.sovereignty.is_min(per_dim_min)
            && self.energy_efficiency.is_min(per_dim_min)
            && self.governance_alignment.is_min(per_dim_min)
    }
}

/// Phase A: component-level scoring interface.
/// Each module (advection kernel, MARL, streaming pipeline, etc.)
/// implements this trait on its own rubric, using domain-specific metrics.
pub trait ComponentEvaluable {
    /// Human-readable identifier (e.g. "advection_kernel_v1").
    fn id(&self) -> &'static str;

    /// Compute the seven-dimension profile for this component
    /// given its configuration and city-bound conditions.
    fn evaluate_component(&self) -> SevenDimProfile;
}

/// Integrated system eligibility, Phase B.
/// Uses component results plus interactions between them.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemEligibility {
    pub profile: SevenDimProfile,
    pub eligible: bool,
    pub notes: String,
}

/// System-level evaluation trait.
/// Implemented by structures that represent an integrated stack (e.g. PhoenixStack).
pub trait SystemEvaluable<C: ComponentEvaluable> {
    fn evaluate_system(&self, components: &[C]) -> SystemEligibility;
}

/// Phoenix-centric deployment thresholds.
/// These are conservative defaults and can be updated by Cybercore governance.
/// They enforce:
/// - A minimum per-dimension score for each component.
/// - A minimum per-dimension score for the integrated system.
/// - A strict cap on risk-of-harm.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct PhoenixThresholds {
    /// Minimum per-dimension score required for each component.
    pub component_min: f32,
    /// Minimum per-dimension score required for the integrated system.
    pub system_min: f32,
    /// Additional constraint: risk-of-harm must be strictly below this value
    /// even if other dimensions are high.
    pub max_risk_of_harm: f32,
}

impl PhoenixThresholds {
    /// Conservative default thresholds for Phoenix.
    /// Adjustments should be done via explicit governance proposals.
    pub fn default() -> Self {
        PhoenixThresholds {
            component_min: 0.75,
            system_min: 0.80,
            max_risk_of_harm: 0.25,
        }
    }
}

/// Simple helper to print or inspect a profile in a consistent order.
/// This is useful for CLI tools or logging, without binding to any UI stack.
pub fn profile_to_rows(profile: &SevenDimProfile) -> Vec<(Dimension, f32)> {
    vec![
        (Dimension::KnowledgeFactor, profile.knowledge_factor.0),
        (Dimension::EcoImpact, profile.eco_impact.0),
        (Dimension::RiskOfHarm, profile.risk_of_harm.0),
        (Dimension::Robustness, profile.robustness.0),
        (Dimension::Sovereignty, profile.sovereignty.0),
        (Dimension::EnergyEfficiency, profile.energy_efficiency.0),
        (Dimension::GovernanceAlignment, profile.governance_alignment.0),
    ]
}
