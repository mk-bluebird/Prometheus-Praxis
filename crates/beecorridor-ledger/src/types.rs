// filename: crates/beecorridor-ledger/src/types.rs

use serde::{Deserialize, Serialize};

/// Marker type for committed ledger access.
#[derive(Debug, Clone, Copy)]
pub struct Committed;

/// Marker type for staging/uncommitted access (not used by query builders).
#[derive(Debug, Clone, Copy)]
pub struct Staging;

/// Strongly-typed corridor identifier.
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct CorridorId(pub String);

/// Risk planes relevant to bee corridors.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum BeeRiskPlane {
    MechanicalContact,
    Emf,
    Acoustic,
    Thermal,
    Chemical,
}

/// Normalized risk coordinate in [0,1].
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct RiskCoordinate(pub f32);

/// Bee risk vector across planes.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BeeRiskVector {
    pub corridor_id: CorridorId,
    pub r_contact: RiskCoordinate,
    pub r_emf: RiskCoordinate,
    pub r_acoustic: RiskCoordinate,
    pub r_thermal: RiskCoordinate,
    pub r_chemical: RiskCoordinate,
}

/// Lyapunov residual for the corridor.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct LyapunovResidual(pub f32);

/// Telemetry snapshot admitted to the committed ledger.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BeeTelemetrySnapshot {
    pub snapshot_id: String,
    pub corridor_id: CorridorId,
    pub timestamp_utc: i64,
    pub location_cell: String,
    pub classified_bee_count: i32,
    pub emf_level: f32,
    pub thermal_delta: f32,
    pub chemical_index: f32,
}
