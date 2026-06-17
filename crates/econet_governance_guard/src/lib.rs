// Filename: crates/econet_governance_guard/src/lib.rs
// Destination: crates/econet_governance_guard/src/lib.rs

#![forbid(unsafe_code)]

use std::path::PathBuf;

use econet_governance_spine::{
    load_expected_schema, BlastRadius, GovernanceSpine, KerResidual, LaneFilter, LaneGuard,
    LaneGuardInputs, Mt6883Guard, Mt6883GuardInputs, PlaneWeight, SpineError,
};

#[derive(Debug)]
pub struct GovernanceGuard {
    spine: GovernanceSpine,
}

impl GovernanceGuard {
    pub fn new(db_path: PathBuf) -> Result<Self, SpineError> {
        let expected = load_expected_schema();
        let spine = GovernanceSpine::open(&db_path, expected)?;
        Ok(GovernanceGuard { spine })
    }

    pub fn lane_transition_allowed_prod(
        &self,
        shard_id: &str,
        now_utc: i64,
    ) -> Result<bool, SpineError> {
        let lane_status = self.spine.get_lane_status(shard_id)?;
        let inputs = LaneGuardInputs {
            lane_status,
            filter: LaneFilter::ExactProd,
            now_utc,
        };
        let result = LaneGuard::check(inputs);
        Ok(result.admissible)
    }

    pub fn lane_transition_allowed_expprod(
        &self,
        shard_id: &str,
        now_utc: i64,
    ) -> Result<bool, SpineError> {
        let lane_status = self.spine.get_lane_status(shard_id)?;
        let inputs = LaneGuardInputs {
            lane_status,
            filter: LaneFilter::ExactExpProd,
            now_utc,
        };
        let result = LaneGuard::check(inputs);
        Ok(result.admissible)
    }

    pub fn mt6883_workload_allowed(
        &self,
        shard_id: &str,
    ) -> Result<bool, SpineError> {
        let ker: KerResidual = self.spine.get_ker_residual(shard_id)?;
        let lane = self.spine.get_lane_status(shard_id)?;
        let blast: BlastRadius = self.spine.get_blast_radius(shard_id)?;
        let plane_weights: Vec<PlaneWeight> = self.spine.get_plane_weights(shard_id)?;
        let inputs = Mt6883GuardInputs {
            ker,
            lane,
            blast,
            plane_weights,
        };
        let result = Mt6883Guard::check(inputs);
        Ok(result.allowed)
    }
}
