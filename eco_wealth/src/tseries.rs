// filename: eco_wealth/src/tseries.rs

use serde::{Deserialize, Serialize};
use crate::model::EcoWealthSnapshot;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoWealthTimePoint {
    pub t_index: i64,
    pub snapshot: EcoWealthSnapshot,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoWealthSeries {
    pub points: Vec<EcoWealthTimePoint>,
}
