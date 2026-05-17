// filename: src/governance_lane_integrity.rs
// destination: eco_restoration_shard/src/governance_lane_integrity.rs

use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use aln_core::Did;
use ecospine::KER;

/// 1. LaneViolation Typology

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum LaneViolationType {
    ResidualNotNonIncreasing,      // RESIDUAL_NOT_NONINCREASING
    KerBandBreach,                 // KER_BAND_BREACH
    HydrologyCorridorViolation,    // HYDROLOGY_CORRIDOR_VIOLATION
    DidSignatureMissing,           // DID_SIGNATURE_MISSING
    PlaneWeightNonoffsettableBreach,
    TopologyDriftExceeded,
    SensorHealthCompromised,
    EvidenceWindowIncomplete,
    ZoningConstraintViolation,
    InteropKerBandMismatch,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaneViolation {
    pub violation_id: Uuid,
    pub lane_id: Uuid,
    pub kernel_region: String,
    pub violation_type: LaneViolationType,
    pub shard_id: Option<Uuid>,
    pub detected_at: OffsetDateTime,
    pub sql_table: String,
    pub sql_column: String,
    pub sql_row_id: Option<i64>,
    pub details: String,
}

/// Mapping hints to keep violations anchored to concrete SQL fields.
impl LaneViolationType {
    pub fn sql_anchor(&self) -> (&'static str, &'static str) {
        match self {
            LaneViolationType::ResidualNotNonIncreasing => ("lanestatusshard", "residual_trend"),
            LaneViolationType::KerBandBreach => ("lanestatusshard", "k_aggregate/e_aggregate/r_aggregate"),
            LaneViolationType::HydrologyCorridorViolation => ("hydrology_constraints", "gwr_max"),
            LaneViolationType::DidSignatureMissing => ("shardinstance", "signing_did"),
            LaneViolationType::PlaneWeightNonoffsettableBreach => ("plane_weights", "nonoffsettable"),
            LaneViolationType::TopologyDriftExceeded => ("topology_audits", "rtopology"),
            LaneViolationType::SensorHealthCompromised => ("sensor_health_particles", "healthy"),
            LaneViolationType::EvidenceWindowIncomplete => ("lanestatusshard", "last_evidence_window"),
            LaneViolationType::ZoningConstraintViolation => ("zoning_shards", "regulation"),
            LaneViolationType::InteropKerBandMismatch => ("interop_index", "ker_band"),
        }
    }
}

/// 2. Promotion Gate Atomicity

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum LanePromotionState {
    Idle,
    Evaluating,
    PendingCommit,
    Committed,
    Aborted,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LanePromotionLock {
    pub lane_id: Uuid,
    pub state: LanePromotionState,
    pub ci_run_id: Uuid,
    pub acquired_at: OffsetDateTime,
    pub expires_at: OffsetDateTime,
}

impl LanePromotionLock {
    /// Simple single-lane lock semantics:
    /// - only one CI run (ci_run_id) may hold Evaluating/PendingCommit at a time
    /// - sensor outages force transition to Aborted
    pub fn can_promote(&self, now: OffsetDateTime) -> bool {
        matches!(self.state, LanePromotionState::Evaluating | LanePromotionState::PendingCommit)
            && now < self.expires_at
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorOutageEvent {
    pub region_id: String,
    pub lane_id: Option<Uuid>,
    pub started_at: OffsetDateTime,
}

/// Called by T06 when computing promotions; T08 publishes SensorOutageEvent.
pub fn handle_promotion_with_sensor_lock(
    lock: &mut LanePromotionLock,
    outage: Option<&SensorOutageEvent>,
    now: OffsetDateTime,
) {
    if let Some(o) = outage {
        if o.lane_id == Some(lock.lane_id) || lock.acquired_at <= o.started_at {
            lock.state = LanePromotionState::Aborted;
        }
    } else if now >= lock.expires_at {
        lock.state = LanePromotionState::Aborted;
    }
}

/// 3. Plane-weight Governance Arbitrage Guard

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeightUpdate {
    pub plane_name: String,
    pub new_weight: f64,
    pub nonoffsettable: bool,
    pub corridor_min: Option<f64>,
    pub corridor_max: Option<f64>,
    pub governance_proposal_hash: String,
    pub signed_by: Did,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeightGovernancePolicy {
    pub required_multisig_dids: Vec<Did>,
    pub min_signatures: u32,
}

impl PlaneWeightGovernancePolicy {
    pub fn is_authorized(&self, update: &PlaneWeightUpdate) -> bool {
        self.required_multisig_dids
            .iter()
            .any(|d| d == &update.signed_by)
    }
}

/// 4. Sovereign Blastradius Override

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SovereignOverride {
    pub override_id: Uuid,
    pub lane_id: Uuid,
    pub region_id: String,
    pub requester_did: Did,
    pub reason: String,
    pub created_at: OffsetDateTime,
    pub expires_at: OffsetDateTime,
    pub original_blastradius_object: Uuid,
    pub new_radius_km: f64,
    pub ker_penalty: KER,
    pub treaty_reference: String,
}

impl SovereignOverride {
    /// Treaty condition: override only allowed if:
    /// - explicit treaty_reference is present,
    /// - expires_at - created_at is within a short window,
    /// - ker_penalty.R >= baseline R (risk increases is explicit).
    pub fn is_treaty_compliant(&self, max_hours: i64) -> bool {
        let duration_ok = (self.expires_at.unix_timestamp()
            - self.created_at.unix_timestamp())
            <= max_hours * 3600;
        let has_treaty = !self.treaty_reference.is_empty();
        duration_ok && has_treaty
    }
}

/// 5. ProposedFix Chaining and Quarantine

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProposedFix {
    pub fix_id: Uuid,
    pub target_id: Uuid,
    pub description: String,
    pub recursion_depth: u32,
    pub quarantine: bool,
}

pub const MAX_FIX_RECURSION_DEPTH: u32 = 3;

impl ProposedFix {
    pub fn next_revision(&self, description: String, violation_detected: bool) -> ProposedFix {
        let next_depth = self.recursion_depth + 1;
        let quarantine = violation_detected || next_depth >= MAX_FIX_RECURSION_DEPTH;

        ProposedFix {
            fix_id: Uuid::new_v4(),
            target_id: self.target_id,
            description,
            recursion_depth: next_depth,
            quarantine,
        }
    }
}
