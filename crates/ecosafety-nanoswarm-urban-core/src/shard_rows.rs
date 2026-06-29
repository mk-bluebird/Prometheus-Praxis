// Filename: crates/ecosafety-nanoswarm-urban-core/src/shard_rows.rs

use serde::{Deserialize, Serialize};
use crate::types::{KerTriplet, ShardRowBase};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct NanoswarmUrbanShardRow {
    #[serde(flatten)]
    pub base: ShardRowBase,

    // Lyapunov barrier diagnostics
    pub v_barrier_prev: f64,
    pub v_barrier_next: f64,
    pub barrier_step_ok: bool,

    // RoH MPC diagnostics
    pub roh_global_max: f64,
    pub roh_allow: bool,

    // Lipschitz aliasing diagnostics
    pub lipschitz_spatial: f64,
    pub lipschitz_temporal: f64,
    pub aliasing_margin_concentration: f64,

    // FPIC consent diagnostics
    pub fpic_decision: String,
    pub fpic_leaf_token_id: Option<String>,
}

impl NanoswarmUrbanShardRow {
    pub fn new(
        shard_id: String,
        timestamputc: i64,
        object_id: String,
        ker: KerTriplet,
    ) -> Self {
        Self {
            base: ShardRowBase {
                shard_id,
                timestamputc,
                object_id,
                ker,
            },
            v_barrier_prev: 0.0,
            v_barrier_next: 0.0,
            barrier_step_ok: false,
            roh_global_max: 0.0,
            roh_allow: false,
            lipschitz_spatial: 0.0,
            lipschitz_temporal: 0.0,
            aliasing_margin_concentration: 0.0,
            fpic_decision: "UNSET".into(),
            fpic_leaf_token_id: None,
        }
    }
}
