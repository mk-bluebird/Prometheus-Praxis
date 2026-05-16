use serde::{Deserialize, Serialize};
use uuid::Uuid;

use aln_core::Did;

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

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Steward {
    pub steward_id: Uuid,
    pub did: Did,
    pub role_type: RoleType,
    pub region_id: String,
    pub responsibilities: Vec<String>,
    pub governance_spine_node_id: Option<Uuid>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoActionDescriptor {
    pub action_id: Uuid,
    pub region_id: String,
    pub keywords: Vec<String>, // e.g. ["tree_corridor","irrigation","block"]
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

        matches.sort_by(|a, b| b.score.partial_cmp(&a.score).unwrap_or(std::cmp::Ordering::Equal));
        matches
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConflictResolution {
    pub conflict_id: Uuid,
    pub action_id: Uuid,
    pub status: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VotingRound {
    pub round_id: Uuid,
    pub conflict_id: Uuid,
    pub governance_lane: String,
}
