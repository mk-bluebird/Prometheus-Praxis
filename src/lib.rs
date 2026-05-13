// filename: eco_response_shard/src/lib.rs
// destination: eco_restoration_shard/eco_response_shard/src/lib.rs

#![forbid(unsafe_code)]

pub mod invariants;
pub mod types;
pub mod plane_weights;

use invariants::{
    check_plane_noncompensation, check_uncertainty_monotonicity, compute_residualscore, compute_vt,
    InvariantError, KerSnapshot, PerPlaneResiduals, PlaneWeights, RiskCoords,
};

/// Marker trait for non-actuating workloads: read-only, pure computation.
pub trait NonActuatingWorkload {
    fn run(&self, conn: &rusqlite::Connection) -> Result<(), WorkloadError>;
}

#[derive(Debug, thiserror::Error)]
pub enum WorkloadError {
    #[error("SQL error: {0}")]
    Sql(#[from] rusqlite::Error),
    #[error("Invariant error: {0}")]
    Invariant(#[from] InvariantError),
    #[error("Invalid data: {0}")]
    Invalid(String),
}
