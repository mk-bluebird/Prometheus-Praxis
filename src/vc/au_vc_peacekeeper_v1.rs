// src/vc/au_vc_peacekeeper_v1.rs
// Rust 2024 crate module for au.vc.peacekeeper.v1, targeting mk-bluebird/eco_restoration_shard

#![feature(rust_2024)]
#![forbid(unsafe_code)]

use core::fmt;

// Scalar in [0.0, 1.0] for normalized scores (de-escalation, trust, etc.)
#[derive(Clone, Copy, PartialEq)]
pub struct Norm01(pub f32);

impl Norm01 {
    pub fn new(x: f32) -> Self {
        Self(x.clamp(0.0, 1.0))
    }

    pub fn get(self) -> f32 {
        self.0
    }
}

impl fmt::Debug for Norm01 {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:.4}", self.0)
    }
}

// Core VC envelope fields, reused across au.vc.*.v1 families.
#[derive(Debug, Clone)]
pub struct VcEnvelope {
    pub vcid: String,
    pub auidref: String,
    pub issuerdid: String,
    pub subjectdid: String,
    pub schemaref: String,
    pub validfrom_epoch: i64,
    pub validuntil_epoch: i64,
    pub revocationslot: String,
}

// Peacekeeper role enumeration.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PeacekeeperRole {
    CommunityMediator,
    TransitDeescalator,
    CivicObserver,
    EnvironmentalSteward,
    DigitalRightsGuardian,
}

// Contexts where the credential is meant to apply.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PeaceContext {
    Transit,
    CivicSpace,
    RetailBasics,
    PublicEvents,
    OnlineCommunity,
}

// Rights and neurorights flags that must be true.
#[derive(Debug, Clone)]
pub struct PeacekeeperRightsFloor {
    pub no_exclusion_basic_services: bool,
    pub noneurocoercion: bool,
    pub noscore_from_inner_state: bool,
    pub revocable_at_will: bool,
    pub antispeculation_kc: bool,
}

// Core peacekeeper metrics, normalized where possible.
#[derive(Debug, Clone)]
pub struct PeacekeeperMetrics {
    pub deescalation_success: Norm01,
    pub accurate_reporting: Norm01,
    pub mediation_frequency_per_week: f32,
    pub eco_contribution_norm: Norm01,
    pub inclusive_behavior_norm: Norm01,
}

// Audit and evidence references.
#[derive(Debug, Clone)]
pub struct PeacekeeperEvidence {
    pub evidence_bundle_id: String,
    pub last_audit_epoch: i64,
    pub audit_hex_trace: String,
}

// Main VC type for au.vc.peacekeeper.v1.
#[derive(Debug, Clone)]
pub struct AuVcPeacekeeperV1 {
    pub envelope: VcEnvelope,
    pub role: PeacekeeperRole,
    pub primary_context: PeaceContext,
    pub rights_floor: PeacekeeperRightsFloor,
    pub metrics: PeacekeeperMetrics,
    pub evidence: PeacekeeperEvidence,
}

#[derive(Debug)]
pub enum VcValidationError {
    InvalidSchemaRef,
    InvalidEpochRange,
    RightsFloorViolation,
    MetricOutOfRange,
    MissingEvidence,
}

impl AuVcPeacekeeperV1 {
    pub fn new(envelope: VcEnvelope,
               role: PeacekeeperRole,
               primary_context: PeaceContext,
               rights_floor: PeacekeeperRightsFloor,
               metrics: PeacekeeperMetrics,
               evidence: PeacekeeperEvidence) -> Result<Self, VcValidationError> {
        if envelope.schemaref != "au.vc.peacekeeper.v1" {
            return Err(VcValidationError::InvalidSchemaRef);
        }

        if envelope.validuntil_epoch < envelope.validfrom_epoch {
            return Err(VcValidationError::InvalidEpochRange);
        }

        if !rights_floor.no_exclusion_basic_services
            || !rights_floor.noneurocoercion
            || !rights_floor.noscore_from_inner_state
            || !rights_floor.revocable_at_will
            || !rights_floor.antispeculation_kc
        {
            return Err(VcValidationError::RightsFloorViolation);
        }

        if metrics.mediation_frequency_per_week < 0.0 {
            return Err(VcValidationError::MetricOutOfRange);
        }

        if evidence.evidence_bundle_id.is_empty() || evidence.audit_hex_trace.is_empty() {
            return Err(VcValidationError::MissingEvidence);
        }

        Ok(Self {
            envelope,
            role,
            primary_context,
            rights_floor,
            metrics,
            evidence,
        })
    }
}

// Guard interface for kiosks/POS consuming peacekeeper VC.
#[derive(Debug, Clone)]
pub struct PosGuardDecision {
    pub allow: bool,
    pub reason: String,
}

pub fn evaluate_peacekeeper_for_basic_service(vc: &AuVcPeacekeeperV1,
                                              service_is_basic: bool) -> PosGuardDecision {
    if service_is_basic {
        if vc.rights_floor.no_exclusion_basic_services {
            return PosGuardDecision {
                allow: true,
                reason: "Basic service, non-exclusion floor enforced".into(),
            };
        } else {
            return PosGuardDecision {
                allow: true,
                reason: "Basic service, but rights_floor missing; defaulting to allow".into(),
            };
        }
    }

    PosGuardDecision {
        allow: true,
        reason: "Non-basic service; peacekeeper VC used only for logging and incentives".into(),
    }
}

pub fn evaluate_peacekeeper_incentives(vc: &AuVcPeacekeeperV1) -> f32 {
    let d = vc.metrics.deescalation_success.get();
    let r = vc.metrics.accurate_reporting.get();
    let e = vc.metrics.eco_contribution_norm.get();
    let i = vc.metrics.inclusive_behavior_norm.get();
    let m = vc.metrics.mediation_frequency_per_week;

    let base = 0.4 * d + 0.2 * r + 0.2 * e + 0.2 * i;
    let freq_term = (m.min(7.0)) / 7.0;
    (base + 0.1 * freq_term).min(1.0)
}
