use serde::{Deserialize, Serialize};

use crate::models::RegionalEcoLedgerParticle;
use ecospine::{KER, Residual, RiskCoord, CorridorBands};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerContext {
    pub ker_coefficients: Vec<KerCoefficient>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerCoefficient {
    pub action_type: String,
    pub k_weight: f64,
    pub e_weight: f64,
    pub r_weight: f64,
}

pub trait KerScorable {
    fn compute_ker(&self, ctx: &KerContext) -> KER;
}

impl KerScorable for RegionalEcoLedgerParticle {
    fn compute_ker(&self, ctx: &KerContext) -> KER {
        let coeff = ctx
            .ker_coefficients
            .iter()
            .find(|c| c.action_type == self.action_type)
            .cloned()
            .unwrap_or(KerCoefficient {
                action_type: self.action_type.clone(),
                k_weight: 0.0,
                e_weight: 0.0,
                r_weight: 0.0,
            });

        let k = coeff.k_weight;
        let e = coeff.e_weight;
        let r = coeff.r_weight;

        KER { k, e, r }
    }
}
