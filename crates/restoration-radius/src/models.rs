use serde::{Deserialize, Serialize};
use time::OffsetDateTime;
use uuid::Uuid;

use ecospine::RiskCoord;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RestorationRadius {
    pub radius_id: Uuid,
    pub center_node_id: Uuid,
    pub radius_km: f64,
    pub pollutant_mass_removed_kg: f64,
    pub karmadelta: f64,
    pub r_gw: RiskCoord,
    pub r_gw_ci95: f64,
    pub calibration_score: f64,  // 0..1
    pub computed_at: OffsetDateTime,
    pub basin_id: String,
}
