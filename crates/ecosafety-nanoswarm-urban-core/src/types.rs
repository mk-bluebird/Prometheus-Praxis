// Filename: crates/ecosafety-nanoswarm-urban-core/src/types.rs

use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct CorridorBands {
    pub var_id: &'static str,
    pub units: &'static str,
    pub safe: f64,
    pub gold: f64,
    pub hard: f64,
    pub weight: f64,
    pub lyap_channel: u8,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct RiskCoord {
    /// Normalized risk in [0, 1].
    pub r: f64,
    /// Uncertainty or variance estimate.
    pub sigma: f64,
    /// Corridor bands for this coordinate.
    pub bands: CorridorBands,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Residual {
    /// Lyapunov-style residual V_t.
    pub vt: f64,
    /// Risk coordinates contributing to V_t.
    pub coords: Vec<RiskCoord>,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct KerTriplet {
    /// Knowledge-factor in [0, 1].
    pub k: f64,
    /// Eco-impact in [0, 1].
    pub e: f64,
    /// Risk-of-harm in [0, 1].
    pub r: f64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShardRowBase {
    pub shard_id: String,
    pub timestamputc: i64,
    pub object_id: String,
    pub ker: KerTriplet,
}
