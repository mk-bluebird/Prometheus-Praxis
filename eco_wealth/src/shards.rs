// filename: eco_wealth/src/shards.rs

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InvestmentProposal {
    pub proposal_id: String,
    pub steward_did: String,
    pub region_code: String,
    pub ts_created_utc: String,
    pub principal_amount: f64,
    pub principal_unit: String,
    pub lane_requested: String,
    pub eco_rationale: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RestorationBond {
    pub bond_id: String,
    pub steward_did: String,
    pub region_code: String,
    pub ts_issued_utc: String,
    pub ts_maturity_utc: String,
    pub principal_amount: f64,
    pub principal_unit: String,
    pub collateral_amount: f64,
    pub collateral_unit: String,
    pub staking_ratio: f64,
}
