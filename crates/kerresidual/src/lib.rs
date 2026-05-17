// filename: src/lib.rs
// destination: eco_restoration_shard/crates/kerresidual/src/lib.rs

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RiskVector {
    pub r_energy: f32,
    pub r_hydraulic: f32,
    pub r_biology: f32,
    pub r_carbon: f32,
    pub r_materials: f32,
    pub r_biodiversity: f32,
    pub r_data_quality: f32,
    pub r_topology: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeights {
    pub w_energy: f32,
    pub w_hydraulic: f32,
    pub w_biology: f32,
    pub w_carbon: f32,
    pub w_materials: f32,
    pub w_biodiversity: f32,
    pub w_data_quality: f32,
    pub w_topology: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerSnapshot {
    pub k: f32,
    pub e: f32,
    pub r: f32,
    pub v_t: f32,
}

pub fn compute_residual(weights: &PlaneWeights, rv: &RiskVector) -> f32 {
    weights.w_energy * rv.r_energy * rv.r_energy
        + weights.w_hydraulic * rv.r_hydraulic * rv.r_hydraulic
        + weights.w_biology * rv.r_biology * rv.r_biology
        + weights.w_carbon * rv.r_carbon * rv.r_carbon
        + weights.w_materials * rv.r_materials * rv.r_materials
        + weights.w_biodiversity * rv.r_biodiversity * rv.r_biodiversity
        + weights.w_data_quality * rv.r_data_quality * rv.r_data_quality
        + weights.w_topology * rv.r_topology * rv.r_topology
}

pub fn compute_k(k: f32) -> f32 {
    k.clamp(0.0, 1.0)
}

pub fn compute_e(e: f32) -> f32 {
    e.clamp(0.0, 1.0)
}

pub fn compute_r(r: f32) -> f32 {
    r.clamp(0.0, 1.0)
}

pub fn check_safestep(v_t_before: f32, v_t_after: f32) -> bool {
    v_t_after <= v_t_before
}
