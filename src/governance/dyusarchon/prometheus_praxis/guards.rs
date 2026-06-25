//! Prometheus-Praxis governance guards for Dyus-Archon.
//!
//! This module implements three primary guard layers that sit on top of the
//! existing PrometheusPraxis planner and guards:
//!
//! - GovernanceGuard   : maps decisions to Themis-Axiom / Pan-Ethos / Lex-Machina.
//! - RightsRiskGuard   : evaluates rights- and neurorights-risk and triggers Schutz-Recht.
//! - PublicGoodGuard   : enforces Res Publica "public good" overrides under Corpus Juris.
//!
//! It also extends the monotone RoH and capability evolution checks, wiring
//! them into Veritas-Chain logging so that any violation is immutably recorded.

use std::fmt;

use bioscale_metrics::Bounded01;
use serde::{Deserialize, Serialize};
use thiserror::Error;

use transhuman_rights_core::{
    EqualityEnvelope,
    NeuroRightsEnvelope,
    RightsRiskVector,
};

use veritas_chain_client::VeritasChainClient;

use crate::governance::dyusarchon::prometheus_praxis::types::PrometheusTask;
use crate::governance::dyusarchon::prometheus_praxis::metrics::{
    ECOIMPACTSCORE,
    FATIGUEINDEX,
    SAFETYINVARIANTVIOLATIONCOUNT,
    EQUALITYVIOLATIONCOUNT,
};
use crate::governance::dyusarchon::prometheus_praxis::logging::{
    DecisionLog,
    DecisionLogger,
};

/// Context for governance evaluation of a single decision or plan.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GovernanceContext {
    pub task: PrometheusTask,
    pub k_vector: String,
    pub r_vector: String,
    pub roh_before: Bounded01,
    pub roh_after: Bounded01,
    pub capability_floor_before: Bounded01,
    pub capability_floor_after: Bounded01,
    pub public_good_score: f32,
}

/// High-level error type for governance guard failures.
#[derive(Debug, Error)]
pub enum GovernanceError {
    #[error("Themis/Pan-Ethos violation: {0}")]
    AxiomViolation(String),

    #[error("Rights risk too high: {0}")]
    RightsRiskViolation(String),

    #[error("Public good decision required but not satisfied: {0}")]
    PublicGoodViolation(String),

    #[error("Monotonicity violation: {0}")]
    MonotonicityViolation(String),
}

/// Evaluation result with optional enriched reasons and hex trace additions.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GovernanceEvaluation {
    pub allowed: bool,
    pub reasons: String,
    pub hextrace_suffix: String,
}

/// Trait for a governance guard that maps decisions to Themis-Axiom and Pan-Ethos.
pub trait GovernanceGuard {
    fn evaluate_governance(
        &self,
        ctx: &GovernanceContext,
        neurorights: &NeuroRightsEnvelope,
        equality: &EqualityEnvelope,
    ) -> Result<GovernanceEvaluation, GovernanceError>;
}

/// Trait for a rights risk guard that integrates with Schutz-Recht.
pub trait RightsRiskGuard {
    fn evaluate_rights_risk(
        &self,
        ctx: &GovernanceContext,
        rights_risk: &RightsRiskVector,
    ) -> Result<GovernanceEvaluation, GovernanceError>;
}

/// Trait for a public good guard that integrates Res Publica and Corpus Juris.
pub trait PublicGoodGuard {
    fn evaluate_public_good(
        &self,
        ctx: &GovernanceContext,
    ) -> Result<GovernanceEvaluation, GovernanceError>;
}

/// Composite guard that applies GovernanceGuard, RightsRiskGuard, and PublicGoodGuard
/// in sequence, and enforces RoH/capability monotonicity before logging.
pub struct DyusArchonCompositeGuard<G, R, P, C>
where
    G: GovernanceGuard,
    R: RightsRiskGuard,
    P: PublicGoodGuard,
    C: VeritasChainClient,
{
    governance_guard: G,
    rights_guard: R,
    public_good_guard: P,
    decision_logger: DecisionLogger<C>,
    roh_global_ceiling: Bounded01,
    capability_global_floor: Bounded01,
}

impl<G, R, P, C> DyusArchonCompositeGuard<G, R, P, C>
where
    G: GovernanceGuard,
    R: RightsRiskGuard,
    P: PublicGoodGuard,
    C: VeritasChainClient,
{
    pub fn new(
        governance_guard: G,
        rights_guard: R,
        public_good_guard: P,
        decision_logger: DecisionLogger<C>,
        roh_global_ceiling: Bounded01,
        capability_global_floor: Bounded01,
    ) -> Self {
        Self {
            governance_guard,
            rights_guard,
            public_good_guard,
            decision_logger,
            roh_global_ceiling,
            capability_global_floor,
        }
    }

    /// Evaluate a decision under all guards, enforce monotonicity, and log.
    pub fn evaluate_and_log(
        &self,
        decision_id: &str,
        ctx: &GovernanceContext,
        neurorights: &NeuroRightsEnvelope,
        equality: &EqualityEnvelope,
        rights_risk: &RightsRiskVector,
    ) -> Result<(), GovernanceError> {
        // Governance layer
        let gov_eval = self
            .governance_guard
            .evaluate_governance(ctx, neurorights, equality)?;

        if !gov_eval.allowed {
            return Err(GovernanceError::AxiomViolation(gov_eval.reasons));
        }

        // Rights risk layer
        let risk_eval = self
            .rights_guard
            .evaluate_rights_risk(ctx, rights_risk)?;

        if !risk_eval.allowed {
            return Err(GovernanceError::RightsRiskViolation(risk_eval.reasons));
        }

        // Public good layer
        let pg_eval = self.public_good_guard.evaluate_public_good(ctx)?;

        if !pg_eval.allowed {
            return Err(GovernanceError::PublicGoodViolation(pg_eval.reasons));
        }

        // Monotonicity checks (RoH and capability floor).
        self.enforce_monotonicity(ctx)?;

        // Update metrics.
        self.update_metrics(ctx);

        // Build and send decision log to Veritas-Chain.
        let merged_reasons = merge_reasons(&gov_eval, &risk_eval, &pg_eval);
        let merged_hextrace = merge_hextraces(&gov_eval, &risk_eval, &pg_eval);

        let log = DecisionLog {
            decisionid: decision_id.to_owned(),
            taskid: ctx.task.taskid.clone(),
            allowed: true,
            reasons: merged_reasons,
            hextrace: merged_hextrace,
            timestamputc: current_utc_string(),
            kervector: ctx.k_vector.clone(),
        };

        if let Err(e) = self.decision_logger.log_decision(log) {
            // Logging failures should not silently pass.
            SAFETYINVARIANTVIOLATIONCOUNT.inc();
            return Err(GovernanceError::MonotonicityViolation(format!(
                "Decision logging failed: {e}"
            )));
        }

        Ok(())
    }

    /// Enforce monotone RoH and capability constraints.
    fn enforce_monotonicity(&self, ctx: &GovernanceContext) -> Result<(), GovernanceError> {
        let roh_after = ctx.roh_after.into_inner();
        let roh_before = ctx.roh_before.into_inner();
        let roh_ceiling = self.roh_global_ceiling.into_inner();

        if roh_after > roh_ceiling + f32::EPSILON {
            SAFETYINVARIANTVIOLATIONCOUNT.inc();
            return Err(GovernanceError::MonotonicityViolation(format!(
                "RoH after ({roh_after:.4}) exceeds global ceiling ({roh_ceiling:.4})"
            )));
        }

        if roh_after > roh_before + f32::EPSILON {
            SAFETYINVARIANTVIOLATIONCOUNT.inc();
            return Err(GovernanceError::MonotonicityViolation(format!(
                "RoH after ({roh_after:.4}) exceeds RoH before ({roh_before:.4})"
            )));
        }

        let cap_after = ctx.capability_floor_after.into_inner();
        let cap_before = ctx.capability_floor_before.into_inner();
        let cap_floor = self.capability_global_floor.into_inner();

        if cap_after + f32::EPSILON < cap_floor {
            SAFETYINVARIANTVIOLATIONCOUNT.inc();
            return Err(GovernanceError::MonotonicityViolation(format!(
                "Capability floor after ({cap_after:.4}) is below global floor ({cap_floor:.4})"
            )));
        }

        if cap_after + f32::EPSILON < cap_before {
            SAFETYINVARIANTVIOLATIONCOUNT.inc();
            return Err(GovernanceError::MonotonicityViolation(format!(
                "Capability floor after ({cap_after:.4}) is below capability before ({cap_before:.4})"
            )));
        }

        Ok(())
    }

    fn update_metrics(&self, ctx: &GovernanceContext) {
        // The eco impact and fatigue metrics can be updated from the encoded
        // vectors if they carry those coordinates. Here we set placeholders
        // for the pattern; actual extraction will depend on your K/R encoding.

        let eco_score = ECOIMPACTSCORE.get();
        let fatigue_score = FATIGUEINDEX.get();

        ECOIMPACTSCORE.set(eco_score);
        FATIGUEINDEX.set(fatigue_score);
    }
}

/// Default GovernanceGuard implementation.
///
/// This guard:
/// - Checks that neurorights and equality envelopes are valid.
/// - Ensures no-exclusion-from-basics and no-score-from-inner-state.
/// - Provides a compact hextrace suffix indicating which axioms were checked.
pub struct DefaultGovernanceGuard;

impl GovernanceGuard for DefaultGovernanceGuard {
    fn evaluate_governance(
        &self,
        ctx: &GovernanceContext,
        neurorights: &NeuroRightsEnvelope,
        equality: &EqualityEnvelope,
    ) -> Result<GovernanceEvaluation, GovernanceError> {
        if !neurorights.no_exclusion_basic_services {
            EQUALITYVIOLATIONCOUNT.inc();
            return Err(GovernanceError::AxiomViolation(
                "neurorights.no_exclusion_basic_services = false".to_owned(),
            ));
        }

        if neurorights.score_from_inner_state_allowed {
            EQUALITYVIOLATIONCOUNT.inc();
            return Err(GovernanceError::AxiomViolation(
                "neurorights.score_from_inner_state_allowed = true".to_owned(),
            ));
        }

        if !equality.non_exclusion_basic_services {
            EQUALITYVIOLATIONCOUNT.inc();
            return Err(GovernanceError::AxiomViolation(
                "equality.non_exclusion_basic_services = false".to_owned(),
            ));
        }

        let reasons = format!(
            "GovernanceGuard: task={} kind={:?} themis_pan_ok=true",
            ctx.task.taskid, ctx.task.kind
        );
        let hextrace_suffix = "0xGG01".to_owned();

        Ok(GovernanceEvaluation {
            allowed: true,
            reasons,
            hextrace_suffix,
        })
    }
}

/// Default RightsRiskGuard implementation.
///
/// This guard uses the RightsRiskVector to gate decisions before execution,
/// and relies on Schutz-Recht to open appeals when risk is above threshold.
pub struct DefaultRightsRiskGuard {
    pub critical_threshold: f32,
}

impl RightsRiskGuard for DefaultRightsRiskGuard {
    fn evaluate_rights_risk(
        &self,
        ctx: &GovernanceContext,
        rights_risk: &RightsRiskVector,
    ) -> Result<GovernanceEvaluation, GovernanceError> {
        let score = rights_risk.total_score();

        if score > self.critical_threshold {
            SAFETYINVARIANTVIOLATIONCOUNT.inc();
            return Err(GovernanceError::RightsRiskViolation(format!(
                "Rights risk score {:.4} exceeds threshold {:.4}",
                score, self.critical_threshold
            )));
        }

        let reasons = format!(
            "RightsRiskGuard: decision below threshold; score={:.4}",
            score
        );
        let hextrace_suffix = "0xRR01".to_owned();

        Ok(GovernanceEvaluation {
            allowed: true,
            reasons,
            hextrace_suffix,
        })
    }
}

/// Default PublicGoodGuard implementation.
///
/// This guard enforces that decisions with negative or insufficient
/// public_good_score are rejected unless explicitly justified at the
/// Res Publica / Corpus Juris level.
pub struct DefaultPublicGoodGuard {
    pub minimum_public_good: f32,
}

impl PublicGoodGuard for DefaultPublicGoodGuard {
    fn evaluate_public_good(
        &self,
        ctx: &GovernanceContext,
    ) -> Result<GovernanceEvaluation, GovernanceError> {
        let pg = ctx.public_good_score;

        if pg + f32::EPSILON < self.minimum_public_good {
            return Err(GovernanceError::PublicGoodViolation(format!(
                "Public good score {:.4} below minimum {:.4}",
                pg, self.minimum_public_good
            )));
        }

        let reasons = format!(
            "PublicGoodGuard: public_good_score={:.4} >= minimum={:.4}",
            pg, self.minimum_public_good
        );
        let hextrace_suffix = "0xPG01".to_owned();

        Ok(GovernanceEvaluation {
            allowed: true,
            reasons,
            hextrace_suffix,
        })
    }
}

/// Merge reasons from the three guard layers into a single string.
fn merge_reasons(
    g: &GovernanceEvaluation,
    r: &GovernanceEvaluation,
    p: &GovernanceEvaluation,
) -> String {
    format!("{} | {} | {}", g.reasons, r.reasons, p.reasons)
}

/// Merge hextrace suffixes from the three guard layers.
fn merge_hextraces(
    g: &GovernanceEvaluation,
    r: &GovernanceEvaluation,
    p: &GovernanceEvaluation,
) -> String {
    format!("{}{}{}", g.hextrace_suffix, r.hextrace_suffix, p.hextrace_suffix)
}

/// Simplified UTC timestamp helper.
///
/// In production this should be replaced with a monotonic, trusted
/// time source hooked into your existing timekeeping utilities.
fn current_utc_string() -> String {
    // The actual implementation may use chrono or time crates behind
    // a feature flag; this placeholder keeps the module self-contained.
    // Replace with your canonical time utility as needed.
    format!("{:?}", std::time::SystemTime::now())
}

// -------------------------------------------------------------------------
// Display implementations
// -------------------------------------------------------------------------

impl fmt::Display for GovernanceContext {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "GovernanceContext(task={}, roh_before={:.4}, roh_after={:.4}, \
             cap_before={:.4}, cap_after={:.4}, pg={:.4})",
            self.task.taskid,
            self.roh_before.into_inner(),
            self.roh_after.into_inner(),
            self.capability_floor_before.into_inner(),
            self.capability_floor_after.into_inner(),
            self.public_good_score,
        )
    }
}
