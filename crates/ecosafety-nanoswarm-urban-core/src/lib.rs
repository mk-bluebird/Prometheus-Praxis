// Filename: crates/ecosafety-nanoswarm-urban-core/src/lib.rs
// rust-version = "1.85", edition = "2024", license = "MIT OR Apache-2.0"

pub mod types;
pub mod lyapunov_barrier;
pub mod roh_mpc_guard;
pub mod lipschitz_aliasing;
pub mod fpic_consent_kernel;
pub mod shard_rows;

pub use types::{CorridorBands, KerTriplet, Residual, RiskCoord, ShardRowBase};
pub use lyapunov_barrier::{
    LyapunovBarrierCorridors,
    LyapunovBarrierState,
    LyapunovBarrierWeights,
    compute_residual as compute_lyapunov_barrier_residual,
    safestep_barrier,
};
pub use roh_mpc_guard::{LaneRoHProfile, RohGlobalConstraint, RohGuardResult, evaluate_global_roh};
pub use lipschitz_aliasing::{
    SensorSample,
    LipschitzEstimates,
    estimate_spatial_l,
    estimate_temporal_l,
    safe_dt,
    safe_dx,
};
pub use fpic_consent_kernel::{
    ConsentKernel,
    ConsentDecision,
    ConsentError,
};
pub use shard_rows::NanoswarmUrbanShardRow;
