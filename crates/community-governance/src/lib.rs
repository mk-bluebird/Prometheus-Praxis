// crates/community-governance/src/lib.rs

#![forbid(unsafe_code)]
#![deny(warnings)]

//! Community governance core for EcoNet / Prometheus-Praxis.

use aln_core::Did;
use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum RoleType {
    BlockSteward,
    WatershedCouncil,
    CooperativeAdmin,
}

impl RoleType {
    pub fn as_str(&self) -> &'static str {
        match self {
            RoleType::BlockSteward => "block_steward",
            RoleType::WatershedCouncil => "watershed_council",
            RoleType::CooperativeAdmin => "cooperative_admin",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Lane {
    Research,
    Experimental,
    Production,
    Retired,
}

impl Lane {
    pub fn as_str(&self) -> &'static str {
        match self {
            Lane::Research => "RESEARCH",
            Lane::Experimental => "EXPERIMENTAL",
            Lane::Production => "PRODUCTION",
            Lane::Retired => "RETIRED",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct KerScore {
    pub k: f32,
    pub e: f32,
    pub r: f32,
}

impl KerScore {
    pub fn new(k: f32, e: f32, r: f32) -> Self {
        Self { k, e, r }
    }

    pub fn is_well_formed(&self) -> bool {
        self.k.is_finite()
            && self.e.is_finite()
            && self.r.is_finite()
            && (0.0..=1.0).contains(&self.k)
            && (0.0..=1.0).contains(&self.e)
            && (0.0..=1.0).contains(&self.r)
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct GovernanceDecision {
    pub artifact_id: Uuid,
    pub lane: Lane,
    pub ker: KerScore,
    pub ker_deployable: bool,
    pub decided_at: OffsetDateTime,
}

impl GovernanceDecision {
    pub fn new(
        artifact_id: Uuid,
        ker: KerScore,
        thresholds: &KerThresholds,
        decided_at: OffsetDateTime,
    ) -> Self {
        let lane = thresholds.infer_lane(ker);
        let ker_deployable = thresholds.is_deployable(ker);
        Self {
            artifact_id,
            lane,
            ker,
            ker_deployable,
            decided_at,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct KerThresholds {
    pub k_min_prod: f32,
    pub e_min_prod: f32,
    pub r_max_prod: f32,
    pub k_min_exp: f32,
    pub e_min_exp: f32,
    pub r_max_exp: f32,
}

impl KerThresholds {
    pub fn default_phoenix() -> Self {
        Self {
            k_min_prod: 0.90,
            e_min_prod: 0.90,
            r_max_prod: 0.13,
            k_min_exp: 0.80,
            e_min_exp: 0.80,
            r_max_exp: 0.25,
        }
    }

    pub fn is_deployable(&self, ker: KerScore) -> bool {
        ker.is_well_formed()
            && ker.k >= self.k_min_prod
            && ker.e >= self.e_min_prod
            && ker.r <= self.r_max_prod
    }

    pub fn infer_lane(&self, ker: KerScore) -> Lane {
        if !ker.is_well_formed() {
            return Lane::Research;
        }
        if self.is_deployable(ker) {
            Lane::Production
        } else if ker.k >= self.k_min_exp && ker.e >= self.e_min_exp && ker.r <= self.r_max_exp {
            Lane::Experimental
        } else {
            Lane::Research
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Steward {
    pub steward_id: Uuid,
    pub did: Did,
    pub role_type: RoleType,
    pub region_id: String,
    pub responsibilities: Vec<String>,
    pub governance_spine_node_id: Option<Uuid>,
}

impl Steward {
    pub fn new<S: Into<String>>(
        steward_id: Uuid,
        did: Did,
        role_type: RoleType,
        region_id: S,
        responsibilities: Vec<String>,
        governance_spine_node_id: Option<Uuid>,
    ) -> Self {
        Self {
            steward_id,
            did,
            role_type,
            region_id: region_id.into(),
            responsibilities,
            governance_spine_node_id,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoActionDescriptor {
    pub action_id: Uuid,
    pub region_id: String,
    pub keywords: Vec<String>,
}

impl EcoActionDescriptor {
    pub fn new<S: Into<String>>(action_id: Uuid, region_id: S, keywords: Vec<String>) -> Self {
        Self {
            action_id,
            region_id: region_id.into(),
            keywords,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StewardMatch {
    pub steward: Steward,
    pub score: f64,
}

pub trait ResponsibilityAssignment {
    fn matches_action(&self, action: &EcoActionDescriptor) -> bool;
}

impl ResponsibilityAssignment for Steward {
    fn matches_action(&self, action: &EcoActionDescriptor) -> bool {
        if self.region_id != action.region_id {
            return false;
        }
        self.responsibilities
            .iter()
            .any(|r| action.keywords.iter().any(|k| k == r))
    }
}

pub struct StewardRouter;

impl StewardRouter {
    pub fn rank_stewards(
        stewards: &[Steward],
        action: &EcoActionDescriptor,
    ) -> Vec<StewardMatch> {
        let mut matches: Vec<StewardMatch> = stewards
            .iter()
            .filter(|s| s.matches_action(action))
            .map(|s| {
                let overlap = s
                    .responsibilities
                    .iter()
                    .filter(|r| action.keywords.iter().any(|k| k == *r))
                    .count() as f64;
                StewardMatch {
                    steward: s.clone(),
                    score: overlap,
                }
            })
            .collect();

        matches.sort_by(|a, b| {
            b.score
                .partial_cmp(&a.score)
                .unwrap_or(std::cmp::Ordering::Equal)
        });
        matches
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConflictResolution {
    pub conflict_id: Uuid,
    pub action_id: Uuid,
    pub status: String,
}

impl ConflictResolution {
    pub fn new<S: Into<String>>(conflict_id: Uuid, action_id: Uuid, status: S) -> Self {
        Self {
            conflict_id,
            action_id,
            status: status.into(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VotingRound {
    pub round_id: Uuid,
    pub conflict_id: Uuid,
    pub governance_lane: String,
}

impl VotingRound {
    pub fn new<S: Into<String>>(round_id: Uuid, conflict_id: Uuid, governance_lane: S) -> Self {
        Self {
            round_id,
            conflict_id,
            governance_lane: governance_lane.into(),
        }
    }
}
