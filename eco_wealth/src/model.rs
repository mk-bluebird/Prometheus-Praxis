// filename: eco_wealth/src/model.rs

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum EcoWealthUnit {
    EcoWealthPoints,
    TCO2e,
    KwhEq,
    UsdIndexed,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoWealthAmount {
    pub value: f64,
    pub unit: EcoWealthUnit,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PortfolioId(pub String);

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StewardId(pub String);

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoWealthSnapshot {
    pub portfolio_id: PortfolioId,
    pub steward_id: StewardId,
    pub region_code: String,
    pub ts_utc: String,
    pub wealth: EcoWealthAmount,
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub vt: f64,
}
