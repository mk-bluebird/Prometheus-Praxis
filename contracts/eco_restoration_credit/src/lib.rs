// filename: contracts/eco_restoration_credit/src/lib.rs
// destination: https://github.com/mk-bluebird/Prometheus-Praxis/contracts/eco_restoration_credit/src/lib.rs
// crate-type: cdylib
// license: MIT OR Apache-2.0

#![forbid(unsafe_code)]

use cosmwasm_std::{
    entry_point, Addr, Binary, Deps, DepsMut, Env, MessageInfo, Response,
    StdError, StdResult, Uint128,
};
use cosmwasm_std::{attr};
use cosmwasm_schema::cw_serde;

#[cw_serde]
pub struct InstantiateMsg {
    pub admin: String,
}

#[cw_serde]
pub enum ExecuteMsg {
    /// Mint eco-restoration credits tied to a hex and workload, if governance conditions are met.
    MintEcoCredit {
        to: String,
        hex_id: String,
        workload_id: String,
        amount: Uint128,
        delta_v: String,        // serialized scalar or hash of Lyapunov residual change
        ker_hash: String,       // hash of K,E,R data produced off-chain
        risk_shard_id: String,  // e.g., "phoenix.uhi.hex.risk.v1.aln"
    },
    /// Burn credits when restoration work is completed and verified.
    BurnEcoCredit {
        from: String,
        hex_id: String,
        workload_id: String,
        amount: Uint128,
        completion_hash: String, // proof of restoration completion (off-chain attestation)
    },
}

#[cw_serde]
pub enum QueryMsg {
    /// Get credit balance and metadata for an address (optionally filtered by hex_id).
    GetBalance {
        address: String,
        hex_id: Option<String>,
    },
}

/// Internal state: admin address.
#[cw_serde]
pub struct Config {
    pub admin: Addr,
}

#[cw_serde]
pub struct CreditRecord {
    pub hex_id: String,
    pub workload_id: String,
    pub amount: Uint128,
    pub delta_v: String,
    pub ker_hash: String,
    pub risk_shard_id: String,
    pub completed: bool,
}

/// Storage keys.
const CONFIG_KEY: &str = "config";
const BALANCE_KEY_PREFIX: &str = "balance:"; // balance:addr:hex_id:workload_id

fn balance_key(addr: &Addr, hex_id: &str, workload_id: &str) -> String {
    format!("{BALANCE_KEY_PREFIX}{addr}:{hex_id}:{workload_id}")
}

#[entry_point]
pub fn instantiate(
    deps: DepsMut,
    _env: Env,
    _info: MessageInfo,
    msg: InstantiateMsg,
) -> StdResult<Response> {
    let admin_addr = deps.api.addr_validate(&msg.admin)?;
    let cfg = Config { admin: admin_addr };
    let cfg_bin = cosmwasm_std::to_binary(&cfg)?;
    deps.storage.set(CONFIG_KEY.as_bytes(), &cfg_bin);

    Ok(Response::new()
        .add_attribute("action", "instantiate")
        .add_attribute("admin", msg.admin))
}

fn load_config(deps: Deps) -> StdResult<Config> {
    let data = deps.storage.get(CONFIG_KEY.as_bytes()).ok_or_else(|| {
        StdError::generic_err("config not set")
    })?;
    let cfg: Config = cosmwasm_std::from_binary(&Binary::from(data))?;
    Ok(cfg)
}

#[entry_point]
pub fn execute(
    deps: DepsMut,
    _env: Env,
    info: MessageInfo,
    msg: ExecuteMsg,
) -> StdResult<Response> {
    match msg {
        ExecuteMsg::MintEcoCredit {
            to,
            hex_id,
            workload_id,
            amount,
            delta_v,
            ker_hash,
            risk_shard_id,
        } => exec_mint(deps, info, to, hex_id, workload_id, amount, delta_v, ker_hash, risk_shard_id),
        ExecuteMsg::BurnEcoCredit {
            from,
            hex_id,
            workload_id,
            amount,
            completion_hash,
        } => exec_burn(deps, info, from, hex_id, workload_id, amount, completion_hash),
    }
}

fn exec_mint(
    deps: DepsMut,
    info: MessageInfo,
    to: String,
    hex_id: String,
    workload_id: String,
    amount: Uint128,
    delta_v: String,
    ker_hash: String,
    risk_shard_id: String,
) -> StdResult<Response> {
    // Only admin can mint; more complex access control can be layered via multisig.
    let cfg = load_config(deps.as_ref())?;
    if info.sender != cfg.admin {
        return Err(StdError::unauthorized());
    }

    // Governance invariants:
    // - delta_v represents a negative Lyapunov residual change (checked off-chain).
    // - ker_hash and risk_shard_id refer to valid off-chain KER and risk shard data.
    // This contract does not recompute Lyapunov; it assumes off-chain verification.
    if delta_v.is_empty() || ker_hash.is_empty() || risk_shard_id.is_empty() {
        return Err(StdError::generic_err("invalid governance metadata"));
    }

    let to_addr = deps.api.addr_validate(&to)?;
    let key = balance_key(&to_addr, &hex_id, &workload_id);

    let existing = deps.storage.get(key.as_bytes());
    let mut record: CreditRecord = if let Some(bytes) = existing {
        cosmwasm_std::from_binary(&Binary::from(bytes))?
    } else {
        CreditRecord {
            hex_id: hex_id.clone(),
            workload_id: workload_id.clone(),
            amount: Uint128::zero(),
            delta_v: delta_v.clone(),
            ker_hash: ker_hash.clone(),
            risk_shard_id: risk_shard_id.clone(),
            completed: false,
        }
    };

    record.amount = record.amount + amount;
    // Store updated record.
    let rec_bin = cosmwasm_std::to_binary(&record)?;
    deps.storage.set(key.as_bytes(), &rec_bin);

    Ok(Response::new()
        .add_attributes(vec![
            attr("action", "mint_eco_credit"),
            attr("to", to),
            attr("hex_id", hex_id),
            attr("workload_id", workload_id),
            attr("amount", amount),
            attr("risk_shard_id", risk_shard_id),
        ]))
}

fn exec_burn(
    deps: DepsMut,
    info: MessageInfo,
    from: String,
    hex_id: String,
    workload_id: String,
    amount: Uint128,
    completion_hash: String,
) -> StdResult<Response> {
    // Only admin can burn; can be extended with more complex ACL.
    let cfg = load_config(deps.as_ref())?;
    if info.sender != cfg.admin {
        return Err(StdError::unauthorized());
    }

    if completion_hash.is_empty() {
        return Err(StdError::generic_err("missing completion_hash"));
    }

    let from_addr = deps.api.addr_validate(&from)?;
    let key = balance_key(&from_addr, &hex_id, &workload_id);

    let existing = deps.storage.get(key.as_bytes()).ok_or_else(|| {
        StdError::not_found("CreditRecord")
    })?;
    let mut record: CreditRecord = cosmwasm_std::from_binary(&Binary::from(existing))?;

    if record.amount < amount {
        return Err(StdError::generic_err("insufficient eco credits to burn"));
    }

    record.amount = record.amount.checked_sub(amount)?;
    record.completed = true;

    // Store updated record.
    let rec_bin = cosmwasm_std::to_binary(&record)?;
    deps.storage.set(key.as_bytes(), &rec_bin);

    Ok(Response::new()
        .add_attributes(vec![
            attr("action", "burn_eco_credit"),
            attr("from", from),
            attr("hex_id", hex_id),
            attr("workload_id", workload_id),
            attr("amount", amount),
            attr("completion_hash", completion_hash),
        ]))
}

#[cw_serde]
pub struct BalanceResponse {
    pub records: Vec<CreditRecord>,
}

#[entry_point]
pub fn query(deps: Deps, _env: Env, msg: QueryMsg) -> StdResult<Binary> {
    match msg {
        QueryMsg::GetBalance { address, hex_id } => query_balance(deps, address, hex_id),
    }
}

fn query_balance(deps: Deps, address: String, hex_id_filter: Option<String>) -> StdResult<Binary> {
    let addr = deps.api.addr_validate(&address)?;
    let prefix = format!("{BALANCE_KEY_PREFIX}{addr}:");

    let mut records: Vec<CreditRecord> = Vec::new();
    // Simple scan over keys; for larger deployments, use prefixed storage iterators.
    let storage = deps.storage.range(None, None, cosmwasm_std::Order::Ascending);
    for item in storage {
        let (key_bytes, value) = item?;
        let key_str = String::from_utf8_lossy(&key_bytes);
        if !key_str.starts_with(&prefix) {
            continue;
        }
        let rec: CreditRecord = cosmwasm_std::from_binary(&Binary::from(value))?;
        if let Some(ref hid) = hex_id_filter {
            if &rec.hex_id != hid {
                continue;
            }
        }
        records.push(rec);
    }

    let resp = BalanceResponse { records };
    cosmwasm_std::to_binary(&resp)
}
