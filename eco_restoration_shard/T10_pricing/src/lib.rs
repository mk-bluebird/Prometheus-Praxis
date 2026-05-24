// filename: T10_pricing/src/lib.rs

use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum PricingError {
    #[error("invalid base cost: {0}")]
    InvalidBaseCost(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoPricingInput {
    pub base_cost: f64,
    pub r_carbon: f64,
    pub r_energy: f64,
    pub r_biodiversity: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EcoPricingOutput {
    pub adjusted_cost: f64,
    pub carbon_multiplier: f64,
    pub energy_multiplier: f64,
    pub biodiversity_multiplier: f64,
}

pub fn compute_eco_price(input: &EcoPricingInput) -> Result<EcoPricingOutput, PricingError> {
    if input.base_cost < 0.0 {
        return Err(PricingError::InvalidBaseCost(
            "base_cost must be non-negative".to_string(),
        ));
    }

    let m_carbon = 1.0 + input.r_carbon.clamp(0.0, 1.0) * 0.5;
    let m_energy = 1.0 + input.r_energy.clamp(0.0, 1.0) * 0.3;
    let m_biodiv = 1.0 + input.r_biodiversity.clamp(0.0, 1.0) * 0.2;

    let adjusted_cost = input.base_cost * m_carbon * m_energy * m_biodiv;

    Ok(EcoPricingOutput {
        adjusted_cost,
        carbon_multiplier: m_carbon,
        energy_multiplier: m_energy,
        biodiversity_multiplier: m_biodiv,
    })
}
