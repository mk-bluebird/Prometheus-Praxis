// Path: crates/eco-wealth-portfolio/src/ppx_invariants.rs
// Role: Rust-side enforcement of PPX no-identity-classification / no-rights-downgrade-by-metric doctrine.

#![forbid(unsafe_code)]

use std::fmt;

/// Mirrors ALE.IDENTITY.PPX.NOIDENTITYCLASSIFICATION.001
/// PPX doctrine: no identity classification, no rights downgrade by metric.

/// Source of information in a potential flow.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum SourceType {
    BrainDid,
    PseudonymousId,
    DeviceDid,
    DataLaborEventId,
    AggregateCohortId,
}

/// Metric type flowing through the system.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum MetricType {
    SimilarityScore,
    PsychContinuityEvidence,
    ReputationScore,
    HealthRiskScore,
    EcoImpactScore,
    LaborContributionScore,
}

/// Target of a decision or write.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum TargetType {
    AccessDecision,
    AclUpdate,
    NeurorightsFloor,
    BenefitSchedule,
    SanctionSchedule,
    ContinuityEvidenceLog,
    EvolutionWindowAudit,
    GovernanceAuditLog,
}

/// A single information flow triple to be evaluated under PPX doctrine.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct InfoFlow {
    pub source: SourceType,
    pub metric: MetricType,
    pub target: TargetType,
}

/// Violation category for PPX invariants.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum PpxViolationKind {
    /// Flow is explicitly forbidden by doctrine forbidden_flows.
    ForbiddenFlow,
    /// Doctrine forbids any rights downgrade by metric, but this flow implies it.
    RightsDowngradeByMetric,
}

/// Structured violation with enough data for audit / logs.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PpxViolation {
    pub kind: PpxViolationKind,
    pub source: SourceType,
    pub metric: MetricType,
    pub target: TargetType,
    pub message: String,
}

impl fmt::Display for PpxViolation {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "PPX violation {:?}: {:?} via {:?} -> {:?} ({})",
            self.kind, self.source, self.metric, self.target, self.message
        )
    }
}

impl std::error::Error for PpxViolation {}

/// Returns true if this triple is in the hard-coded forbidden set
/// corresponding to DOCTRINE_PPX_CORE.forbidden_flows in ALN.
///
/// Mirrors:
/// - FORBID_BRAINDID_SIMILARITY_TO_ACCESS
/// - FORBID_BRAINDID_SIMILARITY_TO_ACL
/// - FORBID_BRAINDID_CONTINUITY_TO_ACCESS
/// - FORBID_BRAINDID_CONTINUITY_TO_ACL
/// - FORBID_BRAINDID_REPUTATION_TO_SANCTION
/// - FORBID_BRAINDID_HEALTHRISK_TO_SANCTION
/// - FORBID_BRAINDID_LABORSCORE_TO_BENEFIT
fn is_forbidden_flow(flow: &InfoFlow) -> bool {
    use MetricType::*;
    use SourceType::*;
    use TargetType::*;

    match (flow.source, flow.metric, flow.target) {
        // Similarity and continuity metrics from BrainDid into access / ACL
        (BrainDid, SimilarityScore, AccessDecision) => true,
        (BrainDid, SimilarityScore, AclUpdate) => true,
        (BrainDid, PsychContinuityEvidence, AccessDecision) => true,
        (BrainDid, PsychContinuityEvidence, AclUpdate) => true,

        // Reputation / health risk metrics from BrainDid into sanctions
        (BrainDid, ReputationScore, SanctionSchedule) => true,
        (BrainDid, HealthRiskScore, SanctionSchedule) => true,

        // Labor metrics from BrainDid into benefits (no metric-based benefit schedule)
        (BrainDid, LaborContributionScore, BenefitSchedule) => true,

        _ => false,
    }
}

/// Returns true if this flow constitutes a rights downgrade path
/// that should be categorically rejected when doctrine
/// `no_rights_downgrade_by_metric = true`.
///
/// For now, we treat any BrainDid-sourced metric into
/// AccessDecision, AclUpdate, BenefitSchedule, or SanctionSchedule
/// as a potential downgrade path.
fn is_rights_downgrade_path(flow: &InfoFlow) -> bool {
    use SourceType::*;
    use TargetType::*;

    if flow.source != BrainDid {
        return false;
    }

    match flow.target {
        AccessDecision | AclUpdate | BenefitSchedule | SanctionSchedule => true,
        _ => false,
    }
}

/// Core guard: enforce PPX doctrine on a single flow.
///
/// This function is deliberately simple and allocation-light so it can be
/// used in both:
/// - compile-time / CI preflight simulations, and
/// - runtime guards on sensitive decision paths.
///
/// Returns Ok(()) if the flow is permitted under doctrine; Err(PpxViolation) otherwise.
pub fn enforce_ppx_doctrine(flow: InfoFlow) -> Result<(), PpxViolation> {
    if is_forbidden_flow(&flow) {
        return Err(PpxViolation {
            kind: PpxViolationKind::ForbiddenFlow,
            source: flow.source,
            metric: flow.metric,
            target: flow.target,
            message: "Flow is explicitly forbidden by PPX doctrine forbidden_flows".to_string(),
        });
    }

    // Doctrine flag: no_rights_downgrade_by_metric = true.
    if is_rights_downgrade_path(&flow) {
        return Err(PpxViolation {
            kind: PpxViolationKind::RightsDowngradeByMetric,
            source: flow.source,
            metric: flow.metric,
            target: flow.target,
            message: "PPX doctrine forbids any rights downgrade by metric from BrainDid".to_string(),
        });
    }

    Ok(())
}

/// Convenience helper for call sites that want a boolean-style check.
pub fn ppx_allows(flow: InfoFlow) -> bool {
    enforce_ppx_doctrine(flow).is_ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn brain_similarity_to_access_is_forbidden() {
        let flow = InfoFlow {
            source: SourceType::BrainDid,
            metric: MetricType::SimilarityScore,
            target: TargetType::AccessDecision,
        };
        let res = enforce_ppx_doctrine(flow);
        assert!(res.is_err());
        let err = res.err().unwrap();
        assert_eq!(err.kind, PpxViolationKind::ForbiddenFlow);
    }

    #[test]
    fn brain_healthrisk_to_sanction_is_forbidden() {
        let flow = InfoFlow {
            source: SourceType::BrainDid,
            metric: MetricType::HealthRiskScore,
            target: TargetType::SanctionSchedule,
        };
        let res = enforce_ppx_doctrine(flow);
        assert!(res.is_err());
        let err = res.err().unwrap();
        assert_eq!(err.kind, PpxViolationKind::ForbiddenFlow);
    }

    #[test]
    fn brain_labor_to_benefit_is_forbidden() {
        let flow = InfoFlow {
            source: SourceType::BrainDid,
            metric: MetricType::LaborContributionScore,
            target: TargetType::BenefitSchedule,
        };
        let res = enforce_ppx_doctrine(flow);
        assert!(res.is_err());
        let err = res.err().unwrap();
        assert_eq!(err.kind, PpxViolationKind::ForbiddenFlow);
    }

    #[test]
    fn brain_reputation_to_neurorights_floor_is_rejected_as_downgrade_path() {
        let flow = InfoFlow {
            source: SourceType::BrainDid,
            metric: MetricType::ReputationScore,
            target: TargetType::NeurorightsFloor,
        };
        // This exact triple is not in forbidden_flows, but is a downgrade path.
        let res = enforce_ppx_doctrine(flow);
        assert!(res.is_err());
        let err = res.err().unwrap();
        assert_eq!(err.kind, PpxViolationKind::RightsDowngradeByMetric);
    }

    #[test]
    fn cohort_reputation_to_governance_audit_is_allowed() {
        let flow = InfoFlow {
            source: SourceType::AggregateCohortId,
            metric: MetricType::ReputationScore,
            target: TargetType::GovernanceAuditLog,
        };
        assert!(enforce_ppx_doctrine(flow).is_ok());
    }
}
