// filename: contracts/eco_restoration_credit/src/msg.rs

use cosmwasm_schema::cw_serde;
use cosmwasm_std::Uint128;

#[cw_serde]
pub struct InstantiateMsg {
    pub admin: String,
}

#[cw_serde]
pub enum ExecuteMsg {
    /// Mint credits tied to a hex and workload, if Lyapunov invariants are satisfied.
    MintEcoCredit {
        to: String,
        hex_id: String,
        workload_id: String,
        amount: Uint128,
        delta_v: String,        // serialized scalar or hash
        ker_hash: String,       // hash of K,E,R data
        risk_shard_id: String,  // e.g., "phoenix.uhi.hex.risk.v1.aln"
    },
    /// Burn credits when restoration work is completed and verified.
    BurnEcoCredit {
        from: String,
        hex_id: String,
        workload_id: String,
        amount: Uint128,
        completion_hash: String, // proof of restoration completion
    },
}

#[cw_serde]
pub enum QueryMsg {
    /// Get credit balance and linked hex/workload metadata.
    GetBalance {
        address: String,
        hex_id: Option<String>,
    },
}
