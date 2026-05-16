// crates/plane-weights/src/models.rs
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeight {
    pub plane_name: String,
    pub weight: f64,
    pub nonoffsettable: bool,
    pub corridor_min: Option<f64>,
    pub corridor_max: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeightsSnapshot {
    pub snapshot_hash: String,
}
