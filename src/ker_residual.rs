// filename src/ker_residual.rs
// destination eco_restoration_shard/src/ker_residual.rs

use serde::{Deserialize, Serialize};

/// PlaneWeights holds weights and non-offsettable flags for each risk plane.
/// r_j values are expected to be in [0, 1].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeightEntry {
    pub plane_id: String,
    pub weight: f64,
    pub non_offsettable: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeights {
    pub entries: Vec<PlaneWeightEntry>,
    pub w_topology: f64,
}

/// ShardInstance carries per-plane risk coordinates and topology drift.
/// This mirrors shardinstance and PlaneWeightsShard2026v1 semantics.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShardInstance {
    pub r_energy: Option<f64>,
    pub r_hydrology_mar: Option<f64>,
    pub r_biodiversity: Option<f64>,
    pub r_carbon: Option<f64>,
    pub r_materials: Option<f64>,
    pub r_data_quality: Option<f64>,
    pub r_topology: Option<f64>,
}

fn clamp_unit(x: f64) -> f64 {
    if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

fn lookup_plane_value(shard: &ShardInstance, plane_id: &str) -> Option<f64> {
    match plane_id {
        "energy" => shard.r_energy,
        "hydrologyMAR" => shard.r_hydrology_mar,
        "biodiversity" => shard.r_biodiversity,
        "carbon" => shard.r_carbon,
        "materials" => shard.r_materials,
        "dataquality" => shard.r_data_quality,
        "topology" => shard.r_topology,
        _ => None,
    }
}

/// Compute Lyapunov residual V_t = sum_j w_j * r_j^2 + w_topology * r_topology^2.
/// Non-actuating: pure function, no I/O, no side effects.
pub fn compute_vt_residual(shard: &ShardInstance, plane_weights: &PlaneWeights) -> f64 {
    let mut vt = 0.0_f64;

    for entry in &plane_weights.entries {
        if let Some(raw_rj) = lookup_plane_value(shard, &entry.plane_id) {
            let rj = clamp_unit(raw_rj);
            vt += entry.weight * rj * rj;
        }
    }

    if let Some(rt) = shard.r_topology {
        let rt_clamped = clamp_unit(rt);
        vt += plane_weights.w_topology * rt_clamped * rt_clamped;
    }

    vt
}
