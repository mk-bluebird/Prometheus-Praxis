use serde::{Deserialize, Serialize};
use uuid::Uuid;

use ecospine::KER;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CostBreakdown {
    pub cost_currency: String,   // e.g. "USD"
    pub capex_per_unit: f64,
    pub opex_per_unit: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CoBenefit {
    pub metric: String,          // "CO2_avoided_kg", "cooling_degC", "biodiversity_index_points"
    pub mean: f64,
    pub std_dev: f64,
    pub unit: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoPricingShard {
    pub shard_id: Uuid,
    pub intervention_id: String,
    pub cost_per_unit: CostBreakdown,
    pub benefits: Vec<CoBenefit>,
    pub ker: KER,
}
