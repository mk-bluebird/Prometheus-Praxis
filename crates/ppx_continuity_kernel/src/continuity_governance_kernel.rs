//! continuity_governance_kernel
//!
//! Read-only continuity and neurorights governance facade on top of the
//! `ContinuityKernel` data access layer.
//!
//! This module:
//! - Aggregates continuity evidence and system well-being components.
//! - Resolves user continuity preferences for minimum acceptable scores.
//! - Binds neuroright corridor specs into effective protection/risk bands.
//! - Produces a non-actuating governance decision (Allow, Warn, Stop) plus
//!   a compact JSON descriptor for MCP/agents.
//!
//! Invariants:
//! - No identity classification, no role inference from metrics.
//! - No rights downgrade by metric: governance never uses lower continuity
//!   or corridor scores to reduce rights; at worst it triggers Warn or Stop.
//! - Read-only: no writes to SQLite, no actuator commands, no side effects
//!   beyond returning values.

use crate::{
    ContinuityKernel,
    KernelError,
    NeurorightCorridorSpec,
    PsychContinuityEvidence,
    SystemWellBeingComponent,
    UserContinuityPreference,
};
use serde::{Deserialize, Serialize};

/// Non-actuating continuity decision for a subject/system/context.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ContinuityDecision {
    /// All continuity and neuroright checks pass with sufficient evidence.
    Allow,
    /// Conditions are near thresholds, evidence is sparse, or minor
    /// neuroright bands are at limit; human review recommended.
    Warn,
    /// Continuity or neuroright corridors are breached; stop and appeal.
    Stop,
}

/// Governance evaluation context.
///
/// This is the minimal input required to evaluate continuity and neuroright
/// corridors for a given augmented citizen and system context.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContinuityGovernanceContext {
    /// Subject DID (must exist in `ppx_braindid` table).
    pub subject_did: String,
    /// System identifier (must exist in `ppx_system` table).
    pub system_id: String,
    /// Context tag (e.g. "HC-SESSION", "AUG-UPGRADE", "CITY-OPS").
    pub context_tag: String,
    /// Optional RFC3339 lower bound for continuity evidence window.
    pub from_utc: Option<String>,
    /// Optional RFC3339 upper bound for continuity evidence window.
    pub to_utc: Option<String>,
    /// Optional override for minimum acceptable continuity score; if `None`,
    /// user preference (if any) is used, otherwise a conservative default.
    pub override_min_score: Option<f64>,
    /// Optional minimum number of continuity evidence rows required before
    /// `Allow` can be considered.
    pub min_evidence_count_for_allow: Option<u32>,
}

/// Aggregate statistics for continuity evidence over a window.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContinuityAggregate {
    pub avg_score: f64,
    pub min_score: f64,
    pub max_score: f64,
    pub evidence_count: u32,
}

/// Aggregate statistics for system well-being components in a context.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemWellBeingAggregate {
    pub avg_value: f64,
    pub min_value: f64,
    pub max_value: f64,
    pub component_count: u32,
}

/// Effective neuroright band for a given corridor spec.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EffectiveNeurorightBand {
    pub id: String,
    pub right_name: String,
    pub min_protection_level: f64,
    pub max_risk_tolerance: f64,
}

/// JSON result envelope for governance evaluation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContinuityGovernanceResult {
    pub decision: ContinuityDecision,
    pub continuity: ContinuityAggregate,
    pub wellbeing: SystemWellBeingAggregate,
    pub corridors: Vec<EffectiveNeurorightBand>,
    pub preferred_min_score: f64,
    pub subject_did: String,
    pub system_id: String,
    pub context_tag: String,
}

/// High-level governance facade on top of `ContinuityKernel`.
#[derive(Debug)]
pub struct ContinuityGovernanceKernel {
    inner: ContinuityKernel,
}

impl ContinuityGovernanceKernel {
    /// Construct a governance kernel from an existing continuity kernel.
    pub fn new(inner: ContinuityKernel) -> Self {
        Self { inner }
    }

    /// Internal helper: aggregate continuity evidence for a window.
    fn aggregate_psych_continuity(
        &self,
        ctx: &ContinuityGovernanceContext,
        limit: u32,
    ) -> Result<ContinuityAggregate, KernelError> {
        let evidence: Vec<PsychContinuityEvidence> = self
            .inner
            .list_psych_continuity_evidence_for_subject(
                &ctx.subject_did,
                ctx.from_utc.as_deref(),
                ctx.to_utc.as_deref(),
                limit,
            )?;

        if evidence.is_empty() {
            // No evidence: treat as zeroed aggregate; downstream logic will
            // handle sparse evidence conditions.
            return Ok(ContinuityAggregate {
                avg_score: 0.0,
                min_score: 0.0,
                max_score: 0.0,
                evidence_count: 0,
            });
        }

        let mut sum = 0.0;
        let mut min = f64::MAX;
        let mut max = f64::MIN;
        for e in &evidence {
            let s = e.score;
            sum += s;
            if s < min {
                min = s;
            }
            if s > max {
                max = s;
            }
        }

        let count = evidence.len() as u32;
        let avg = sum / (count as f64);

        Ok(ContinuityAggregate {
            avg_score: avg,
            min_score: min,
            max_score: max,
            evidence_count: count,
        })
    }

    /// Internal helper: aggregate system well-being components.
    fn aggregate_system_wellbeing(
        &self,
        ctx: &ContinuityGovernanceContext,
    ) -> Result<SystemWellBeingAggregate, KernelError> {
        let comps: Vec<SystemWellBeingComponent> = self
            .inner
            .list_system_wellbeing_components(&ctx.system_id, &ctx.context_tag)?;

        if comps.is_empty() {
            return Ok(SystemWellBeingAggregate {
                avg_value: 0.0,
                min_value: 0.0,
                max_value: 0.0,
                component_count: 0,
            });
        }

        let mut sum = 0.0;
        let mut min = f64::MAX;
        let mut max = f64::MIN;
        for c in &comps {
            let v = c.value;
            sum += v;
            if v < min {
                min = v;
            }
            if v > max {
                max = v;
            }
        }

        let count = comps.len() as u32;
        let avg = sum / (count as f64);

        Ok(SystemWellBeingAggregate {
            avg_value: avg,
            min_value: min,
            max_value: max,
            component_count: count,
        })
    }

    /// Internal helper: resolve preferred continuity floor.
    ///
    /// - If `override_min_score` is provided, use it (clamped into [0.0, 1.0]).
    /// - Otherwise, consult `ppx_usercontinuitypreference` if present.
    /// - Fallback to a conservative default of 0.7 if no preference exists.
    fn resolve_preferred_min_score(
        &self,
        ctx: &ContinuityGovernanceContext,
    ) -> Result<f64, KernelError> {
        if let Some(v) = ctx.override_min_score {
            return Ok(Self::clamp01(v));
        }

        let opt_pref: Option<UserContinuityPreference> = self
            .inner
            .get_user_continuity_preference(&ctx.subject_did)?;

        if let Some(pref) = opt_pref {
            Ok(Self::clamp01(pref.preferred_min_score))
        } else {
            Ok(0.7)
        }
    }

    /// Internal helper: fetch neuroright corridor specs and project into bands.
    fn effective_neuroright_bands(
        &self,
        context_tag: &str,
    ) -> Result<Vec<EffectiveNeurorightBand>, KernelError> {
        let specs: Vec<NeurorightCorridorSpec> = self
            .inner
            .list_neuroright_corridors_for_context(context_tag)?;

        let bands = specs
            .into_iter()
            .map(|s| EffectiveNeurorightBand {
                id: s.id,
                right_name: s.right_name,
                min_protection_level: Self::clamp01(s.min_protection_level),
                max_risk_tolerance: Self::clamp01(s.max_risk_tolerance),
            })
            .collect();

        Ok(bands)
    }

    /// Core governance evaluation: continuity + neuroright corridors.
    ///
    /// This function is deliberately non-actuating; it only reads from the
    /// SQLite kernel and returns a decision + descriptor. It never writes,
    /// never triggers actuators, and never performs identity classification.
    pub fn evaluate(
        &self,
        ctx: &ContinuityGovernanceContext,
    ) -> Result<ContinuityGovernanceResult, KernelError> {
        // Bound on how many rows to aggregate for continuity evidence; we
        // reuse the same limit for evaluation and JSON reporting.
        let continuity_limit: u32 = 256;

        let continuity = self.aggregate_psych_continuity(ctx, continuity_limit)?;
        let wellbeing = self.aggregate_system_wellbeing(ctx)?;
        let preferred_min_score = self.resolve_preferred_min_score(ctx)?;
        let corridors = self.effective_neuroright_bands(&ctx.context_tag)?;

        let min_evidence_for_allow = ctx.min_evidence_count_for_allow.unwrap_or(3);

        let decision = Self::compute_decision(
            &continuity,
            &wellbeing,
            &corridors,
            preferred_min_score,
            min_evidence_for_allow,
        );

        Ok(ContinuityGovernanceResult {
            decision,
            continuity,
            wellbeing,
            corridors,
            preferred_min_score,
            subject_did: ctx.subject_did.clone(),
            system_id: ctx.system_id.clone(),
            context_tag: ctx.context_tag.clone(),
        })
    }

    /// JSON facade for MCP/agents and CI tooling.
    ///
    /// This is a single-shot helper that takes a context, runs evaluation, and
    /// returns a compact JSON string describing the continuity and neuroright
    /// situation for the subject/system/context triplet.
    pub fn json_evaluate(
        &self,
        ctx: &ContinuityGovernanceContext,
    ) -> Result<String, KernelError> {
        let res = self.evaluate(ctx)?;
        Ok(serde_json::to_string(&res)?)
    }

    /// Clamp a scalar into [0.0, 1.0] to enforce normalized bands.
    fn clamp01(v: f64) -> f64 {
        if v < 0.0 {
            0.0
        } else if v > 1.0 {
            1.0
        } else {
            v
        }
    }

    /// Compute a continuity decision from aggregates and corridor bands.
    ///
    /// This function encodes the core invariants:
    /// - If continuity falls below the preferred floor, decision is at most
    ///   `Warn` and typically `Stop`.
    /// - If neuroright corridors are breached (protection below floor or
    ///   risk above tolerance), decision is `Stop` regardless of continuity.
    /// - Rights are never downgraded by metrics; lower continuity or corridor
    ///   scores cannot produce a more permissive decision than a higher set.
    fn compute_decision(
        continuity: &ContinuityAggregate,
        wellbeing: &SystemWellBeingAggregate,
        corridors: &[EffectiveNeurorightBand],
        preferred_min_score: f64,
        min_evidence_for_allow: u32,
    ) -> ContinuityDecision {
        // Evidence sparsity: if there are too few continuity samples, we
        // default to Warn unless hard breaches force Stop.
        let sparse_evidence = continuity.evidence_count < min_evidence_for_allow;

        // Continuity checks.
        let continuity_below_floor =
            continuity.min_score < preferred_min_score || continuity.avg_score < preferred_min_score;

        // Neurorights corridor checks: we bind system well-being to the bands.
        let mut corridor_breach = false;
        for band in corridors {
            // Simple mapping: treat wellbeing.min_value as a proxy for
            // protection level, and (1 - wellbeing.avg_value) as proxy for
            // risk level. This is deliberately conservative; any low
            // well-being or high risk interpretation yields a breach.
            let protection_level = wellbeing.min_value;
            let risk_level = 1.0 - wellbeing.avg_value;

            if protection_level < band.min_protection_level
                || risk_level > band.max_risk_tolerance
            {
                corridor_breach = true;
                break;
            }
        }

        // Hard breach: neuroright corridor or extremely low continuity.
        if corridor_breach || continuity.min_score < 0.3 {
            return ContinuityDecision::Stop;
        }

        // Continuity below user floor: Warn at minimum.
        if continuity_below_floor {
            return ContinuityDecision::Warn;
        }

        // Sparse evidence: even if scores look good, do not jump to Allow.
        if sparse_evidence {
            return ContinuityDecision::Warn;
        }

        // All checks pass, evidence is sufficient: Allow.
        ContinuityDecision::Allow
    }
}
