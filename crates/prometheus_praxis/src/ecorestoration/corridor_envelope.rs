// FILE: crates/prometheus_praxis/src/ecorestoration/corridor_envelope.rs
// ROLE: Soil/water/habitat corridor constraints for optimization and preflight.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use rust_decimal::Decimal;

use crate::praxisgovernancekernel::ROHCEILING;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum CorridorType {
    Soil,
    WaterChem,
    Habitat,
    HeatBudget,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum ConstraintKind {
    Hard,
    SoftHighPenalty,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CorridorEnvelope {
    pub corridor_id: String,
    pub corridor_type: CorridorType,
    pub region_id: String,
    pub soil_moisture_min: Decimal,
    pub soil_moisture_max: Decimal,
    pub soil_ph_min: f32,
    pub soil_ph_max: f32,
    pub salinity_min: f32,
    pub salinity_max: f32,
    pub nutrient_min: f32,
    pub nutrient_max: f32,
    pub water_table_min: f32,
    pub water_table_max: f32,
    pub contamination_index_max: Decimal,
    pub habitat_continuity_min: Decimal,
    pub biodiversity_floor: Decimal,
    pub roh_ceiling_local: Decimal,
    pub constraint_kind: ConstraintKind,
    pub treaty_ids: Vec<String>,
}

impl CorridorEnvelope {
    pub fn roh_local_leq_global(&self) -> bool {
        self.roh_ceiling_local <= ROHCEILING
    }

    pub fn within_soil_corridor(&self, soil_moisture: Decimal, soil_ph: f32, salinity: f32) -> bool {
        soil_moisture >= self.soil_moisture_min
            && soil_moisture <= self.soil_moisture_max
            && soil_ph >= self.soil_ph_min
            && soil_ph <= self.soil_ph_max
            && salinity >= self.salinity_min
            && salinity <= self.salinity_max
    }
}
